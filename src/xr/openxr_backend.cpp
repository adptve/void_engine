/// @file openxr_backend.cpp
/// @brief OpenXR backend implementation

#include <void_engine/presenter/xr/xr_system.hpp>

#if defined(VOID_HAS_OPENXR)

#include <openxr/openxr.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace void_presenter {
namespace xr {

// =============================================================================
// OpenXR Utilities
// =============================================================================

namespace {

/// Check OpenXR result and throw on error
inline void xr_check(XrResult result, const char* operation) {
    if (XR_FAILED(result)) {
        throw std::runtime_error(std::string("OpenXR error in ") + operation);
    }
}

/// Convert OpenXR pose to our Pose type
inline Pose from_xr_pose(const XrPosef& xr_pose) {
    Pose pose;
    pose.position = {
        xr_pose.position.x,
        xr_pose.position.y,
        xr_pose.position.z
    };
    pose.orientation = {
        xr_pose.orientation.x,
        xr_pose.orientation.y,
        xr_pose.orientation.z,
        xr_pose.orientation.w
    };
    return pose;
}

/// Convert OpenXR FOV to our Fov type
inline Fov from_xr_fov(const XrFovf& xr_fov) {
    return {
        xr_fov.angleLeft,
        xr_fov.angleRight,
        xr_fov.angleUp,
        xr_fov.angleDown
    };
}

/// Convert our reference space type to OpenXR
inline XrReferenceSpaceType to_xr_reference_space(ReferenceSpaceType type) {
    switch (type) {
        case ReferenceSpaceType::View: return XR_REFERENCE_SPACE_TYPE_VIEW;
        case ReferenceSpaceType::Local: return XR_REFERENCE_SPACE_TYPE_LOCAL;
        case ReferenceSpaceType::LocalFloor: return XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
        case ReferenceSpaceType::Stage: return XR_REFERENCE_SPACE_TYPE_STAGE;
        case ReferenceSpaceType::Unbounded: return XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT;
        default: return XR_REFERENCE_SPACE_TYPE_LOCAL;
    }
}

/// Convert our session state to OpenXR state
inline XrSessionState to_xr_session_state(XrSessionState state) {
    switch (state) {
        case XR_SESSION_STATE_IDLE: return XrSessionState::Idle;
        case XR_SESSION_STATE_READY: return XrSessionState::Ready;
        case XR_SESSION_STATE_SYNCHRONIZED: return XrSessionState::Synchronized;
        case XR_SESSION_STATE_VISIBLE: return XrSessionState::Visible;
        case XR_SESSION_STATE_FOCUSED: return XrSessionState::Focused;
        case XR_SESSION_STATE_STOPPING: return XrSessionState::Stopping;
        case XR_SESSION_STATE_LOSS_PENDING: return XrSessionState::LossPending;
        case XR_SESSION_STATE_EXITING: return XrSessionState::Exiting;
        default: return XrSessionState::Unknown;
    }
}

} // anonymous namespace

// =============================================================================
// OpenXR Session
// =============================================================================

class OpenXrSession : public IXrSession {
public:
    OpenXrSession(
        XrInstance instance,
        XrSystemId system_id,
        const XrSessionConfig& config,
        IBackend* graphics_backend)
        : instance_(instance)
        , system_id_(system_id)
        , config_(config)
        , graphics_backend_(graphics_backend) {

        create_session();
        create_reference_space();
        create_swapchains();
        setup_actions();
    }

    ~OpenXrSession() override {
        cleanup();
    }

    [[nodiscard]] XrSessionState state() const override {
        return session_state_;
    }

    [[nodiscard]] const XrSessionConfig& config() const override {
        return config_;
    }

    bool begin() override {
        if (session_state_ != XrSessionState::Ready) {
            return false;
        }

        XrSessionBeginInfo begin_info{XR_TYPE_SESSION_BEGIN_INFO};
        begin_info.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

        XrResult result = xrBeginSession(session_, &begin_info);
        return XR_SUCCEEDED(result);
    }

    void end() override {
        if (session_ != XR_NULL_HANDLE) {
            xrEndSession(session_);
        }
    }

    void request_exit() override {
        if (session_ != XR_NULL_HANDLE) {
            xrRequestExitSession(session_);
        }
    }

    bool wait_frame(XrFrame& out_frame) override {
        XrFrameWaitInfo wait_info{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frame_state{XR_TYPE_FRAME_STATE};

        XrResult result = xrWaitFrame(session_, &wait_info, &frame_state);
        if (XR_FAILED(result)) {
            return false;
        }

        predicted_display_time_ = frame_state.predictedDisplayTime;
        out_frame.should_render = frame_state.shouldRender;
        out_frame.timing.predicted_display_time = frame_state.predictedDisplayTime;
        out_frame.timing.predicted_display_period = frame_state.predictedDisplayPeriod;
        out_frame.timing.frame_begin = std::chrono::steady_clock::now();
        out_frame.frame_number = frame_number_++;

        if (out_frame.should_render) {
            // Get view poses
            locate_views();
            out_frame.views = views_;
            out_frame.head_pose = head_pose_;

            // Get controller input
            sync_actions();
            if (left_controller_.active) {
                out_frame.left_controller = left_controller_;
            }
            if (right_controller_.active) {
                out_frame.right_controller = right_controller_;
            }

            // Get hand tracking if enabled
            if (config_.enable_hand_tracking && hand_tracker_left_ != XR_NULL_HANDLE) {
                out_frame.left_hand = get_hand_tracking_data(Hand::Left);
                out_frame.right_hand = get_hand_tracking_data(Hand::Right);
            }
        }

        out_frame.session_active = (session_state_ == XrSessionState::Focused ||
                                    session_state_ == XrSessionState::Visible);

        return true;
    }

    void begin_frame() override {
        XrFrameBeginInfo begin_info{XR_TYPE_FRAME_BEGIN_INFO};
        xrBeginFrame(session_, &begin_info);
    }

    void end_frame(const XrStereoTargets& /*targets*/) override {
        std::vector<XrCompositionLayerBaseHeader*> layers;

        // Create projection layer
        XrCompositionLayerProjectionView projection_views[2] = {};
        for (int i = 0; i < 2; ++i) {
            projection_views[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            projection_views[i].pose = xr_views_[i].pose;
            projection_views[i].fov = xr_views_[i].fov;
            projection_views[i].subImage.swapchain = (i == 0) ? swapchain_left_ : swapchain_right_;
            projection_views[i].subImage.imageRect.offset = {0, 0};
            projection_views[i].subImage.imageRect.extent = {
                static_cast<int32_t>(views_.view(static_cast<Eye>(i)).width),
                static_cast<int32_t>(views_.view(static_cast<Eye>(i)).height)
            };
        }

        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        layer.space = reference_space_;
        layer.viewCount = 2;
        layer.views = projection_views;
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));

        XrFrameEndInfo end_info{XR_TYPE_FRAME_END_INFO};
        end_info.displayTime = predicted_display_time_;
        end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        end_info.layerCount = static_cast<uint32_t>(layers.size());
        end_info.layers = layers.data();

        xrEndFrame(session_, &end_info);
    }

    [[nodiscard]] XrStereoTargets acquire_swapchain_images() override {
        XrStereoTargets targets;

        // Acquire left eye
        XrSwapchainImageAcquireInfo acquire_info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t left_index, right_index;

        xrAcquireSwapchainImage(swapchain_left_, &acquire_info, &left_index);
        xrAcquireSwapchainImage(swapchain_right_, &acquire_info, &right_index);

        XrSwapchainImageWaitInfo wait_info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait_info.timeout = XR_INFINITE_DURATION;
        xrWaitSwapchainImage(swapchain_left_, &wait_info);
        xrWaitSwapchainImage(swapchain_right_, &wait_info);

        targets.left.width = views_.left.width;
        targets.left.height = views_.left.height;
        targets.left.format = config_.color_format;
        targets.left.array_index = left_index;

        targets.right.width = views_.right.width;
        targets.right.height = views_.right.height;
        targets.right.format = config_.color_format;
        targets.right.array_index = right_index;

        return targets;
    }

    void release_swapchain_images() override {
        XrSwapchainImageReleaseInfo release_info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(swapchain_left_, &release_info);
        xrReleaseSwapchainImage(swapchain_right_, &release_info);
    }

    [[nodiscard]] StereoViews get_views() const override {
        return views_;
    }

    [[nodiscard]] TrackedPose get_head_pose() const override {
        return head_pose_;
    }

    [[nodiscard]] std::optional<ControllerState> get_controller(Hand hand) const override {
        const ControllerState& state = (hand == Hand::Left) ? left_controller_ : right_controller_;
        if (!state.active) {
            return std::nullopt;
        }
        return state;
    }

    [[nodiscard]] std::optional<HandTrackingData> get_hand_tracking(Hand hand) const override {
        if (!config_.enable_hand_tracking) {
            return std::nullopt;
        }
        return get_hand_tracking_data(hand);
    }

    [[nodiscard]] std::optional<StageBounds> get_stage_bounds() const override {
        XrExtent2Df bounds;
        XrResult result = xrGetReferenceSpaceBoundsRect(session_, XR_REFERENCE_SPACE_TYPE_STAGE, &bounds);
        if (XR_FAILED(result)) {
            return std::nullopt;
        }

        StageBounds stage;
        stage.width = bounds.width;
        stage.depth = bounds.height;

        // Generate boundary polygon
        float hw = bounds.width * 0.5f;
        float hd = bounds.height * 0.5f;
        stage.boundary_points = {
            {-hw, 0.0f, -hd},
            { hw, 0.0f, -hd},
            { hw, 0.0f,  hd},
            {-hw, 0.0f,  hd},
        };

        return stage;
    }

    void set_foveation(const FoveatedRenderingConfig& config) override {
        foveation_config_ = config;
        // Would apply foveation via XR_FB_foveation extension
    }

    void trigger_haptic(Hand hand, float amplitude, float duration_seconds) override {
        XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude = amplitude;
        vibration.duration = static_cast<XrDuration>(duration_seconds * 1e9);  // Convert to nanoseconds
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo haptic_info{XR_TYPE_HAPTIC_ACTION_INFO};
        haptic_info.action = haptic_action_;
        haptic_info.subactionPath = (hand == Hand::Left) ? left_hand_path_ : right_hand_path_;

        xrApplyHapticFeedback(session_, &haptic_info,
                             reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    }

    void poll_events() override {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};

        while (xrPollEvent(instance_, &event) == XR_SUCCESS) {
            switch (event.type) {
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    auto* state_event = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
                    session_state_ = to_xr_session_state(state_event->state);

                    if (event_callback_) {
                        XrEvent e;
                        e.type = XrEventType::SessionStateChanged;
                        e.new_session_state = session_state_;
                        event_callback_(e);
                    }
                    break;
                }
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                    if (event_callback_) {
                        XrEvent e;
                        e.type = XrEventType::ReferenceSpaceChanged;
                        event_callback_(e);
                    }
                    break;
                }
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                    if (event_callback_) {
                        XrEvent e;
                        e.type = XrEventType::InteractionProfileChanged;
                        event_callback_(e);
                    }
                    break;
                }
                default:
                    break;
            }
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
    }

    void set_event_callback(XrEventCallback callback) override {
        event_callback_ = std::move(callback);
    }

private:
    void create_session() {
        // Get graphics requirements (simplified - would need actual graphics binding)
        XrSessionCreateInfo create_info{XR_TYPE_SESSION_CREATE_INFO};
        create_info.systemId = system_id_;
        // Would set create_info.next to graphics binding structure

        xr_check(xrCreateSession(instance_, &create_info, &session_), "xrCreateSession");
    }

    void create_reference_space() {
        XrReferenceSpaceCreateInfo create_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        create_info.referenceSpaceType = to_xr_reference_space(config_.primary_reference_space);
        create_info.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};

        xr_check(xrCreateReferenceSpace(session_, &create_info, &reference_space_),
                "xrCreateReferenceSpace");
    }

    void create_swapchains() {
        // Get view configuration views
        uint32_t view_count;
        xrEnumerateViewConfigurationViews(instance_, system_id_,
                                          XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                          0, &view_count, nullptr);

        std::vector<XrViewConfigurationView> config_views(view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        xrEnumerateViewConfigurationViews(instance_, system_id_,
                                          XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                          view_count, &view_count, config_views.data());

        // Get swapchain formats
        uint32_t format_count;
        xrEnumerateSwapchainFormats(session_, 0, &format_count, nullptr);
        std::vector<int64_t> formats(format_count);
        xrEnumerateSwapchainFormats(session_, format_count, &format_count, formats.data());

        // Create swapchains for each eye
        for (uint32_t i = 0; i < std::min(view_count, 2u); ++i) {
            XrSwapchainCreateInfo swapchain_info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                                        XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            swapchain_info.format = formats[0];  // Use first supported format
            swapchain_info.sampleCount = config_.sample_count;
            swapchain_info.width = config_views[i].recommendedImageRectWidth;
            swapchain_info.height = config_views[i].recommendedImageRectHeight;
            swapchain_info.faceCount = 1;
            swapchain_info.arraySize = 1;
            swapchain_info.mipCount = 1;

            XrSwapchain& swapchain = (i == 0) ? swapchain_left_ : swapchain_right_;
            xr_check(xrCreateSwapchain(session_, &swapchain_info, &swapchain), "xrCreateSwapchain");

            // Store dimensions in views
            XrView& view = (i == 0) ? views_.left : views_.right;
            view.width = swapchain_info.width;
            view.height = swapchain_info.height;
        }

        xr_views_.resize(2, {XR_TYPE_VIEW});
    }

    void setup_actions() {
        // Create action set
        XrActionSetCreateInfo action_set_info{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::strcpy(action_set_info.actionSetName, "gameplay");
        std::strcpy(action_set_info.localizedActionSetName, "Gameplay");
        xrCreateActionSet(instance_, &action_set_info, &action_set_);

        // Create hand paths
        xrStringToPath(instance_, "/user/hand/left", &left_hand_path_);
        xrStringToPath(instance_, "/user/hand/right", &right_hand_path_);
        XrPath hand_paths[] = {left_hand_path_, right_hand_path_};

        // Create pose action
        XrActionCreateInfo pose_info{XR_TYPE_ACTION_CREATE_INFO};
        pose_info.actionType = XR_ACTION_TYPE_POSE_INPUT;
        std::strcpy(pose_info.actionName, "hand_pose");
        std::strcpy(pose_info.localizedActionName, "Hand Pose");
        pose_info.countSubactionPaths = 2;
        pose_info.subactionPaths = hand_paths;
        xrCreateAction(action_set_, &pose_info, &pose_action_);

        // Create trigger action
        XrActionCreateInfo trigger_info{XR_TYPE_ACTION_CREATE_INFO};
        trigger_info.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
        std::strcpy(trigger_info.actionName, "trigger");
        std::strcpy(trigger_info.localizedActionName, "Trigger");
        trigger_info.countSubactionPaths = 2;
        trigger_info.subactionPaths = hand_paths;
        xrCreateAction(action_set_, &trigger_info, &trigger_action_);

        // Create grip action
        XrActionCreateInfo grip_info{XR_TYPE_ACTION_CREATE_INFO};
        grip_info.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
        std::strcpy(grip_info.actionName, "grip");
        std::strcpy(grip_info.localizedActionName, "Grip");
        grip_info.countSubactionPaths = 2;
        grip_info.subactionPaths = hand_paths;
        xrCreateAction(action_set_, &grip_info, &grip_action_);

        // Create haptic action
        XrActionCreateInfo haptic_info{XR_TYPE_ACTION_CREATE_INFO};
        haptic_info.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
        std::strcpy(haptic_info.actionName, "haptic");
        std::strcpy(haptic_info.localizedActionName, "Haptic");
        haptic_info.countSubactionPaths = 2;
        haptic_info.subactionPaths = hand_paths;
        xrCreateAction(action_set_, &haptic_info, &haptic_action_);

        // Suggest interaction bindings (simplified - supports multiple profiles)
        suggest_bindings();

        // Create action spaces for poses
        XrActionSpaceCreateInfo space_info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        space_info.action = pose_action_;
        space_info.poseInActionSpace = {{0, 0, 0, 1}, {0, 0, 0}};

        space_info.subactionPath = left_hand_path_;
        xrCreateActionSpace(session_, &space_info, &left_hand_space_);

        space_info.subactionPath = right_hand_path_;
        xrCreateActionSpace(session_, &space_info, &right_hand_space_);

        // Attach action set
        XrSessionActionSetsAttachInfo attach_info{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attach_info.countActionSets = 1;
        attach_info.actionSets = &action_set_;
        xrAttachSessionActionSets(session_, &attach_info);
    }

    void suggest_bindings() {
        // Oculus Touch bindings
        XrPath oculus_profile;
        xrStringToPath(instance_, "/interaction_profiles/oculus/touch_controller", &oculus_profile);

        std::vector<XrActionSuggestedBinding> bindings;

        XrPath left_grip, right_grip, left_trigger, right_trigger;
        xrStringToPath(instance_, "/user/hand/left/input/grip/pose", &left_grip);
        xrStringToPath(instance_, "/user/hand/right/input/grip/pose", &right_grip);
        xrStringToPath(instance_, "/user/hand/left/input/trigger/value", &left_trigger);
        xrStringToPath(instance_, "/user/hand/right/input/trigger/value", &right_trigger);

        bindings.push_back({pose_action_, left_grip});
        bindings.push_back({pose_action_, right_grip});
        bindings.push_back({trigger_action_, left_trigger});
        bindings.push_back({trigger_action_, right_trigger});

        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile = oculus_profile;
        suggested.suggestedBindings = bindings.data();
        suggested.countSuggestedBindings = static_cast<uint32_t>(bindings.size());

        xrSuggestInteractionProfileBindings(instance_, &suggested);
    }

    void locate_views() {
        XrViewLocateInfo locate_info{XR_TYPE_VIEW_LOCATE_INFO};
        locate_info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate_info.displayTime = predicted_display_time_;
        locate_info.space = reference_space_;

        XrViewState view_state{XR_TYPE_VIEW_STATE};
        uint32_t view_count;

        xrLocateViews(session_, &locate_info, &view_state, 2, &view_count, xr_views_.data());

        // Convert to our view format
        for (uint32_t i = 0; i < std::min(view_count, 2u); ++i) {
            XrView& dst = (i == 0) ? views_.left : views_.right;
            dst.eye = static_cast<Eye>(i);
            dst.pose = from_xr_pose(xr_views_[i].pose);
            dst.fov = from_xr_fov(xr_views_[i].fov);
        }

        // Get head pose (average of two views)
        head_pose_.pose.position = {
            (views_.left.pose.position.x + views_.right.pose.position.x) * 0.5f,
            (views_.left.pose.position.y + views_.right.pose.position.y) * 0.5f,
            (views_.left.pose.position.z + views_.right.pose.position.z) * 0.5f
        };
        head_pose_.pose.orientation = views_.left.pose.orientation;  // Use left eye orientation
        head_pose_.position_valid = (view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0;
        head_pose_.orientation_valid = (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0;
    }

    void sync_actions() {
        XrActiveActionSet active_set{};
        active_set.actionSet = action_set_;
        active_set.subactionPath = XR_NULL_PATH;

        XrActionsSyncInfo sync_info{XR_TYPE_ACTIONS_SYNC_INFO};
        sync_info.countActiveActionSets = 1;
        sync_info.activeActionSets = &active_set;

        xrSyncActions(session_, &sync_info);

        // Update controller states
        update_controller(Hand::Left, left_hand_space_, left_controller_);
        update_controller(Hand::Right, right_hand_space_, right_controller_);
    }

    void update_controller(Hand hand, XrSpace space, ControllerState& state) {
        state.hand = hand;
        XrPath subaction = (hand == Hand::Left) ? left_hand_path_ : right_hand_path_;

        // Get pose
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        xrLocateSpace(space, reference_space_, predicted_display_time_, &location);

        state.pose.position_valid = (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
        state.pose.orientation_valid = (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
        state.pose.pose = from_xr_pose(location.pose);
        state.active = state.pose.position_valid && state.pose.orientation_valid;

        // Get trigger value
        XrActionStateFloat trigger_state{XR_TYPE_ACTION_STATE_FLOAT};
        XrActionStateGetInfo get_info{XR_TYPE_ACTION_STATE_GET_INFO};
        get_info.action = trigger_action_;
        get_info.subactionPath = subaction;
        xrGetActionStateFloat(session_, &get_info, &trigger_state);
        state.trigger = trigger_state.currentState;

        if (trigger_state.currentState > 0.5f) {
            state.buttons_pressed = state.buttons_pressed | ControllerButton::Trigger;
        }

        // Get grip value
        XrActionStateFloat grip_state{XR_TYPE_ACTION_STATE_FLOAT};
        get_info.action = grip_action_;
        xrGetActionStateFloat(session_, &get_info, &grip_state);
        state.grip = grip_state.currentState;

        if (grip_state.currentState > 0.5f) {
            state.buttons_pressed = state.buttons_pressed | ControllerButton::Grip;
        }
    }

    [[nodiscard]] HandTrackingData get_hand_tracking_data(Hand hand) const {
        HandTrackingData data;
        data.hand = hand;
        data.active = false;

        // Would use XR_EXT_hand_tracking extension here
        // This is a placeholder - actual implementation needs extension support

        return data;
    }

    void cleanup() {
        if (left_hand_space_ != XR_NULL_HANDLE) xrDestroySpace(left_hand_space_);
        if (right_hand_space_ != XR_NULL_HANDLE) xrDestroySpace(right_hand_space_);
        if (action_set_ != XR_NULL_HANDLE) xrDestroyActionSet(action_set_);
        if (swapchain_left_ != XR_NULL_HANDLE) xrDestroySwapchain(swapchain_left_);
        if (swapchain_right_ != XR_NULL_HANDLE) xrDestroySwapchain(swapchain_right_);
        if (reference_space_ != XR_NULL_HANDLE) xrDestroySpace(reference_space_);
        if (session_ != XR_NULL_HANDLE) xrDestroySession(session_);
    }

    // OpenXR handles
    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId system_id_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace reference_space_ = XR_NULL_HANDLE;
    XrSwapchain swapchain_left_ = XR_NULL_HANDLE;
    XrSwapchain swapchain_right_ = XR_NULL_HANDLE;

    // Action system
    XrActionSet action_set_ = XR_NULL_HANDLE;
    XrAction pose_action_ = XR_NULL_HANDLE;
    XrAction trigger_action_ = XR_NULL_HANDLE;
    XrAction grip_action_ = XR_NULL_HANDLE;
    XrAction haptic_action_ = XR_NULL_HANDLE;
    XrPath left_hand_path_ = XR_NULL_PATH;
    XrPath right_hand_path_ = XR_NULL_PATH;
    XrSpace left_hand_space_ = XR_NULL_HANDLE;
    XrSpace right_hand_space_ = XR_NULL_HANDLE;

    // Hand tracking (optional extension)
    XrHandTrackerEXT hand_tracker_left_ = XR_NULL_HANDLE;
    XrHandTrackerEXT hand_tracker_right_ = XR_NULL_HANDLE;

    // State
    XrSessionConfig config_;
    XrSessionState session_state_ = XrSessionState::Idle;
    StereoViews views_;
    TrackedPose head_pose_;
    ControllerState left_controller_;
    ControllerState right_controller_;
    FoveatedRenderingConfig foveation_config_;
    XrEventCallback event_callback_;

    std::vector<XrView> xr_views_;
    XrTime predicted_display_time_ = 0;
    std::uint64_t frame_number_ = 0;

    IBackend* graphics_backend_ = nullptr;
};

// =============================================================================
// OpenXR System
// =============================================================================

class OpenXrSystem : public IXrSystem {
public:
    explicit OpenXrSystem(const std::string& app_name, std::uint32_t app_version) {
        create_instance(app_name, app_version);
        get_system();
        query_capabilities();
    }

    ~OpenXrSystem() override {
        if (instance_ != XR_NULL_HANDLE) {
            xrDestroyInstance(instance_);
        }
    }

    [[nodiscard]] const XrRuntimeInfo& runtime_info() const override {
        return runtime_info_;
    }

    [[nodiscard]] const XrSystemCapabilities& capabilities() const override {
        return capabilities_;
    }

    [[nodiscard]] bool is_available() const override {
        return instance_ != XR_NULL_HANDLE && system_id_ != XR_NULL_SYSTEM_ID;
    }

    [[nodiscard]] std::unique_ptr<IXrSession> create_session(
        const XrSessionConfig& config,
        IBackend* graphics_backend) override {
        if (!is_available()) {
            return nullptr;
        }
        return std::make_unique<OpenXrSession>(instance_, system_id_, config, graphics_backend);
    }

    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> recommended_resolution() const override {
        return {recommended_width_, recommended_height_};
    }

    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> max_resolution() const override {
        return {max_width_, max_height_};
    }

    [[nodiscard]] std::vector<float> supported_refresh_rates() const override {
        return refresh_rates_;
    }

    bool set_refresh_rate(float /*hz*/) override {
        // Would use XR_FB_display_refresh_rate extension
        return false;
    }

    void poll_events() override {
        // Events are polled per-session
    }

private:
    void create_instance(const std::string& app_name, std::uint32_t app_version) {
        // Query available extensions
        uint32_t ext_count;
        xrEnumerateInstanceExtensionProperties(nullptr, 0, &ext_count, nullptr);
        std::vector<XrExtensionProperties> extensions(ext_count, {XR_TYPE_EXTENSION_PROPERTIES});
        xrEnumerateInstanceExtensionProperties(nullptr, ext_count, &ext_count, extensions.data());

        // Check for required extensions
        std::vector<const char*> enabled_extensions;

        // Graphics extension (platform-specific)
#if defined(_WIN32)
        bool has_d3d11 = false, has_d3d12 = false;
        for (const auto& ext : extensions) {
            if (std::strcmp(ext.extensionName, "XR_KHR_D3D11_enable") == 0) has_d3d11 = true;
            if (std::strcmp(ext.extensionName, "XR_KHR_D3D12_enable") == 0) has_d3d12 = true;
        }
        if (has_d3d12) enabled_extensions.push_back("XR_KHR_D3D12_enable");
        else if (has_d3d11) enabled_extensions.push_back("XR_KHR_D3D11_enable");
#elif defined(__linux__)
        for (const auto& ext : extensions) {
            if (std::strcmp(ext.extensionName, "XR_KHR_vulkan_enable2") == 0) {
                enabled_extensions.push_back("XR_KHR_vulkan_enable2");
                break;
            }
        }
#endif

        // Optional extensions
        for (const auto& ext : extensions) {
            if (std::strcmp(ext.extensionName, "XR_EXT_hand_tracking") == 0) {
                enabled_extensions.push_back("XR_EXT_hand_tracking");
                has_hand_tracking_ = true;
            }
        }

        XrInstanceCreateInfo create_info{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strncpy(create_info.applicationInfo.applicationName, app_name.c_str(), XR_MAX_APPLICATION_NAME_SIZE - 1);
        create_info.applicationInfo.applicationVersion = app_version;
        std::strcpy(create_info.applicationInfo.engineName, "Void Engine");
        create_info.applicationInfo.engineVersion = 1;
        create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
        create_info.enabledExtensionNames = enabled_extensions.data();

        XrResult result = xrCreateInstance(&create_info, &instance_);
        if (XR_FAILED(result)) {
            instance_ = XR_NULL_HANDLE;
            return;
        }

        // Get runtime properties
        XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
        xrGetInstanceProperties(instance_, &props);
        runtime_info_.name = props.runtimeName;
        runtime_info_.version = std::to_string(XR_VERSION_MAJOR(props.runtimeVersion)) + "." +
                                std::to_string(XR_VERSION_MINOR(props.runtimeVersion)) + "." +
                                std::to_string(XR_VERSION_PATCH(props.runtimeVersion));
    }

    void get_system() {
        if (instance_ == XR_NULL_HANDLE) return;

        XrSystemGetInfo get_info{XR_TYPE_SYSTEM_GET_INFO};
        get_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        XrResult result = xrGetSystem(instance_, &get_info, &system_id_);
        if (XR_FAILED(result)) {
            system_id_ = XR_NULL_SYSTEM_ID;
            return;
        }

        runtime_info_.system_type = XrSystemType::HeadMountedVR;
        runtime_info_.system_id = system_id_;

        // Get system properties
        XrSystemProperties sys_props{XR_TYPE_SYSTEM_PROPERTIES};
        xrGetSystemProperties(instance_, system_id_, &sys_props);
        runtime_info_.name = sys_props.systemName;
    }

    void query_capabilities() {
        if (system_id_ == XR_NULL_SYSTEM_ID) return;

        capabilities_.hand_tracking = has_hand_tracking_;
        capabilities_.eye_tracking = false;  // Would check for XR_EXT_eye_gaze_interaction
        capabilities_.foveated_rendering = false;  // Would check for XR_FB_foveation
        capabilities_.passthrough = false;
        capabilities_.spatial_anchors = false;
        capabilities_.scene_understanding = false;
        capabilities_.body_tracking = false;
        capabilities_.max_views = 2;
        capabilities_.max_layer_count = 16;

        // Query view configurations
        uint32_t view_count;
        xrEnumerateViewConfigurationViews(instance_, system_id_,
                                          XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                          0, &view_count, nullptr);

        std::vector<XrViewConfigurationView> views(view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        xrEnumerateViewConfigurationViews(instance_, system_id_,
                                          XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                          view_count, &view_count, views.data());

        if (!views.empty()) {
            recommended_width_ = views[0].recommendedImageRectWidth;
            recommended_height_ = views[0].recommendedImageRectHeight;
            max_width_ = views[0].maxImageRectWidth;
            max_height_ = views[0].maxImageRectHeight;
        }

        // Query reference spaces
        capabilities_.supported_reference_spaces = {
            ReferenceSpaceType::View,
            ReferenceSpaceType::Local,
            ReferenceSpaceType::LocalFloor,
            ReferenceSpaceType::Stage,
        };

        // Query swapchain formats (placeholder - would do actual query)
        capabilities_.supported_swapchain_formats = {
            SurfaceFormat::Rgba8UnormSrgb,
            SurfaceFormat::Bgra8UnormSrgb,
        };

        // Default refresh rates (would query from extension)
        refresh_rates_ = {90.0f};
    }

    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId system_id_ = XR_NULL_SYSTEM_ID;
    XrRuntimeInfo runtime_info_;
    XrSystemCapabilities capabilities_;
    bool has_hand_tracking_ = false;

    std::uint32_t recommended_width_ = 1920;
    std::uint32_t recommended_height_ = 1920;
    std::uint32_t max_width_ = 4096;
    std::uint32_t max_height_ = 4096;
    std::vector<float> refresh_rates_;
};

// =============================================================================
// Factory with OpenXR Support
// =============================================================================

std::unique_ptr<IXrSystem> create_openxr_system(
    const std::string& app_name,
    std::uint32_t app_version) {
    try {
        auto system = std::make_unique<OpenXrSystem>(app_name, app_version);
        if (system->is_available()) {
            return system;
        }
    } catch (...) {
        // OpenXR initialization failed
    }
    return nullptr;
}

} // namespace xr
} // namespace void_presenter

#endif // VOID_HAS_OPENXR
