//! Patch validation - ensures patches are valid before application
//!
//! This module provides comprehensive validation for patches including:
//! - Entity reference validation
//! - Component type registration checks
//! - Namespace permission checks
//! - Value type schema validation
//! - Dependency validation

use crate::namespace::{Namespace, NamespaceId};
use crate::patch::{AssetOp, ComponentOp, EntityOp, EntityRef, Patch, PatchKind};
use crate::value::Value;
use std::collections::{HashMap, HashSet};
use std::sync::Arc;

/// Result of validation
pub type ValidationResult<T = ()> = Result<T, ValidationError>;

/// Errors that can occur during validation
#[derive(Debug, Clone)]
pub enum ValidationError {
    /// Entity does not exist
    EntityNotFound(EntityRef),
    /// Entity already exists
    EntityAlreadyExists(EntityRef),
    /// Component type is not registered
    ComponentTypeNotRegistered(String),
    /// Component does not exist on entity
    ComponentNotFound { entity: EntityRef, component: String },
    /// Component already exists on entity
    ComponentAlreadyExists { entity: EntityRef, component: String },
    /// Namespace does not exist
    NamespaceNotFound(NamespaceId),
    /// Namespace does not have permission
    PermissionDenied { namespace: NamespaceId, operation: String },
    /// Value type does not match schema
    TypeMismatch { expected: String, got: String },
    /// Required field is missing
    MissingField { component: String, field: String },
    /// Value is out of valid range
    ValueOutOfRange { field: String, value: String },
    /// Layer ID is invalid or already exists
    InvalidLayerId(String),
    /// Asset path is invalid
    InvalidAssetPath(String),
    /// Circular dependency detected
    CircularDependency,
    /// Parent entity does not exist
    ParentNotFound(EntityRef),
    /// Invalid hierarchy (would create cycle)
    InvalidHierarchy { child: EntityRef, parent: EntityRef },
    /// Custom validation error
    Custom(String),
}

impl std::fmt::Display for ValidationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::EntityNotFound(entity) => write!(f, "Entity not found: {:?}", entity),
            Self::EntityAlreadyExists(entity) => write!(f, "Entity already exists: {:?}", entity),
            Self::ComponentTypeNotRegistered(ty) => write!(f, "Component type not registered: {}", ty),
            Self::ComponentNotFound { entity, component } => {
                write!(f, "Component '{}' not found on entity {:?}", component, entity)
            }
            Self::ComponentAlreadyExists { entity, component } => {
                write!(f, "Component '{}' already exists on entity {:?}", component, entity)
            }
            Self::NamespaceNotFound(id) => write!(f, "Namespace not found: {}", id),
            Self::PermissionDenied { namespace, operation } => {
                write!(f, "Namespace {} denied permission for: {}", namespace, operation)
            }
            Self::TypeMismatch { expected, got } => {
                write!(f, "Type mismatch: expected {}, got {}", expected, got)
            }
            Self::MissingField { component, field } => {
                write!(f, "Missing required field '{}' in component '{}'", field, component)
            }
            Self::ValueOutOfRange { field, value } => {
                write!(f, "Value '{}' out of range for field '{}'", value, field)
            }
            Self::InvalidLayerId(id) => write!(f, "Invalid layer ID: {}", id),
            Self::InvalidAssetPath(path) => write!(f, "Invalid asset path: {}", path),
            Self::CircularDependency => write!(f, "Circular dependency detected"),
            Self::ParentNotFound(parent) => write!(f, "Parent entity not found: {:?}", parent),
            Self::InvalidHierarchy { child, parent } => {
                write!(f, "Invalid hierarchy: {:?} -> {:?}", child, parent)
            }
            Self::Custom(msg) => write!(f, "{}", msg),
        }
    }
}

impl std::error::Error for ValidationError {}

/// Component schema for type validation
#[derive(Debug, Clone)]
pub struct ComponentSchema {
    /// Component type name
    pub name: String,
    /// Required fields
    pub required_fields: HashMap<String, FieldSchema>,
    /// Optional fields
    pub optional_fields: HashMap<String, FieldSchema>,
}

impl ComponentSchema {
    /// Create a new component schema
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            required_fields: HashMap::new(),
            optional_fields: HashMap::new(),
        }
    }

    /// Add a required field
    pub fn with_required_field(mut self, name: impl Into<String>, schema: FieldSchema) -> Self {
        self.required_fields.insert(name.into(), schema);
        self
    }

    /// Add an optional field
    pub fn with_optional_field(mut self, name: impl Into<String>, schema: FieldSchema) -> Self {
        self.optional_fields.insert(name.into(), schema);
        self
    }

    /// Validate a value against this schema
    pub fn validate(&self, value: &Value) -> ValidationResult {
        let obj = value.as_object().ok_or_else(|| ValidationError::TypeMismatch {
            expected: "Object".to_string(),
            got: format!("{:?}", value),
        })?;

        // Check required fields
        for (field_name, field_schema) in &self.required_fields {
            let field_value = obj.get(field_name).ok_or_else(|| ValidationError::MissingField {
                component: self.name.clone(),
                field: field_name.clone(),
            })?;

            field_schema.validate(field_name, field_value)?;
        }

        // Check optional fields if present
        for (field_name, field_schema) in &self.optional_fields {
            if let Some(field_value) = obj.get(field_name) {
                field_schema.validate(field_name, field_value)?;
            }
        }

        Ok(())
    }
}

/// Schema for a field in a component
#[derive(Debug, Clone)]
pub enum FieldSchema {
    /// Boolean field
    Bool,
    /// Integer field with optional range
    Int { min: Option<i64>, max: Option<i64> },
    /// Float field with optional range
    Float { min: Option<f64>, max: Option<f64> },
    /// String field with optional max length
    String { max_length: Option<usize> },
    /// Vec3 field
    Vec3,
    /// Vec4 field
    Vec4,
    /// Mat4 field
    Mat4,
    /// Array of a specific type
    Array(Box<FieldSchema>),
    /// Object with nested schema
    Object(HashMap<String, FieldSchema>),
    /// Any value type
    Any,
}

impl FieldSchema {
    /// Validate a value against this schema
    pub fn validate(&self, field_name: &str, value: &Value) -> ValidationResult {
        match self {
            Self::Bool => {
                value.as_bool().ok_or_else(|| ValidationError::TypeMismatch {
                    expected: "Bool".to_string(),
                    got: format!("{:?}", value),
                })?;
            }
            Self::Int { min, max } => {
                let val = value.as_int().ok_or_else(|| ValidationError::TypeMismatch {
                    expected: "Int".to_string(),
                    got: format!("{:?}", value),
                })?;

                if let Some(min) = min {
                    if val < *min {
                        return Err(ValidationError::ValueOutOfRange {
                            field: field_name.to_string(),
                            value: val.to_string(),
                        });
                    }
                }

                if let Some(max) = max {
                    if val > *max {
                        return Err(ValidationError::ValueOutOfRange {
                            field: field_name.to_string(),
                            value: val.to_string(),
                        });
                    }
                }
            }
            Self::Float { min, max } => {
                let val = value.as_float().ok_or_else(|| ValidationError::TypeMismatch {
                    expected: "Float".to_string(),
                    got: format!("{:?}", value),
                })?;

                if let Some(min) = min {
                    if val < *min {
                        return Err(ValidationError::ValueOutOfRange {
                            field: field_name.to_string(),
                            value: val.to_string(),
                        });
                    }
                }

                if let Some(max) = max {
                    if val > *max {
                        return Err(ValidationError::ValueOutOfRange {
                            field: field_name.to_string(),
                            value: val.to_string(),
                        });
                    }
                }
            }
            Self::String { max_length } => {
                let s = value.as_str().ok_or_else(|| ValidationError::TypeMismatch {
                    expected: "String".to_string(),
                    got: format!("{:?}", value),
                })?;

                if let Some(max) = max_length {
                    if s.len() > *max {
                        return Err(ValidationError::ValueOutOfRange {
                            field: field_name.to_string(),
                            value: format!("{} chars", s.len()),
                        });
                    }
                }
            }
            Self::Vec3 => {
                value.as_vec3().ok_or_else(|| ValidationError::TypeMismatch {
                    expected: "Vec3".to_string(),
                    got: format!("{:?}", value),
                })?;
            }
            Self::Vec4 => {
                if let Value::Vec4(_) = value {
                    // OK
                } else {
                    return Err(ValidationError::TypeMismatch {
                        expected: "Vec4".to_string(),
                        got: format!("{:?}", value),
                    });
                }
            }
            Self::Mat4 => {
                if let Value::Mat4(_) = value {
                    // OK
                } else {
                    return Err(ValidationError::TypeMismatch {
                        expected: "Mat4".to_string(),
                        got: format!("{:?}", value),
                    });
                }
            }
            Self::Array(item_schema) => {
                if let Value::Array(arr) = value {
                    for item in arr {
                        item_schema.validate(field_name, item)?;
                    }
                } else {
                    return Err(ValidationError::TypeMismatch {
                        expected: "Array".to_string(),
                        got: format!("{:?}", value),
                    });
                }
            }
            Self::Object(fields) => {
                let obj = value.as_object().ok_or_else(|| ValidationError::TypeMismatch {
                    expected: "Object".to_string(),
                    got: format!("{:?}", value),
                })?;

                for (field, schema) in fields {
                    if let Some(val) = obj.get(field) {
                        schema.validate(field, val)?;
                    }
                }
            }
            Self::Any => {
                // Any value is valid
            }
        }

        Ok(())
    }
}

/// Context for validation
pub struct ValidationContext {
    /// Existing entities
    entities: HashSet<EntityRef>,
    /// Component types per entity
    entity_components: HashMap<EntityRef, HashSet<String>>,
    /// Registered component schemas
    component_schemas: HashMap<String, ComponentSchema>,
    /// Registered namespaces
    namespaces: HashMap<NamespaceId, Arc<Namespace>>,
    /// Existing layer IDs
    layers: HashSet<String>,
    /// Entity hierarchy (child -> parent)
    hierarchy: HashMap<EntityRef, EntityRef>,
}

impl ValidationContext {
    /// Create a new validation context
    pub fn new() -> Self {
        Self {
            entities: HashSet::new(),
            entity_components: HashMap::new(),
            component_schemas: HashMap::new(),
            namespaces: HashMap::new(),
            layers: HashSet::new(),
            hierarchy: HashMap::new(),
        }
    }

    /// Register a namespace
    pub fn register_namespace(&mut self, namespace: Arc<Namespace>) {
        self.namespaces.insert(namespace.id, namespace);
    }

    /// Register a component schema
    pub fn register_component_schema(&mut self, schema: ComponentSchema) {
        self.component_schemas.insert(schema.name.clone(), schema);
    }

    /// Register an existing entity
    pub fn register_entity(&mut self, entity: EntityRef) {
        self.entities.insert(entity);
        self.entity_components.entry(entity).or_insert_with(HashSet::new);
    }

    /// Register a component on an entity
    pub fn register_component(&mut self, entity: EntityRef, component: String) {
        self.entity_components
            .entry(entity)
            .or_insert_with(HashSet::new)
            .insert(component);
    }

    /// Register a layer
    pub fn register_layer(&mut self, layer_id: String) {
        self.layers.insert(layer_id);
    }

    /// Check if entity exists
    pub fn entity_exists(&self, entity: &EntityRef) -> bool {
        self.entities.contains(entity)
    }

    /// Check if component exists on entity
    pub fn component_exists(&self, entity: &EntityRef, component: &str) -> bool {
        self.entity_components
            .get(entity)
            .map_or(false, |comps| comps.contains(component))
    }

    /// Validate a single patch
    pub fn validate_patch(&self, patch: &Patch) -> ValidationResult {
        // Check namespace exists
        let namespace = self.namespaces.get(&patch.source).ok_or_else(|| {
            ValidationError::NamespaceNotFound(patch.source)
        })?;

        // Validate based on patch kind
        match &patch.kind {
            PatchKind::Entity(ep) => self.validate_entity_patch(ep, namespace)?,
            PatchKind::Component(cp) => self.validate_component_patch(cp, namespace)?,
            PatchKind::Layer(lp) => self.validate_layer_patch(lp, namespace)?,
            PatchKind::Asset(ap) => self.validate_asset_patch(ap, namespace)?,
            PatchKind::Hierarchy(hp) => self.validate_hierarchy_patch(hp, namespace)?,
            PatchKind::Camera(cp) => self.validate_camera_patch(cp, namespace)?,
        }

        Ok(())
    }

    /// Validate a camera patch
    fn validate_camera_patch(
        &self,
        patch: &crate::patch::CameraPatch,
        namespace: &Namespace,
    ) -> ValidationResult {
        // Check namespace can modify the entity
        if !namespace.can_modify(patch.entity.namespace) {
            return Err(ValidationError::PermissionDenied {
                namespace: namespace.id,
                operation: "modify camera entity in different namespace".to_string(),
            });
        }

        Ok(())
    }

    /// Validate a hierarchy patch
    fn validate_hierarchy_patch(
        &self,
        patch: &crate::patch::HierarchyPatch,
        namespace: &Namespace,
    ) -> ValidationResult {
        // Check namespace can modify the entity
        if !namespace.can_modify(patch.entity.namespace) {
            return Err(ValidationError::PermissionDenied {
                namespace: namespace.id,
                operation: "modify entity hierarchy in different namespace".to_string(),
            });
        }

        // Validate based on operation
        match &patch.op {
            crate::patch::HierarchyOp::SetParent { parent } => {
                // Check can access parent's namespace
                if !namespace.can_access(parent.namespace) {
                    return Err(ValidationError::PermissionDenied {
                        namespace: namespace.id,
                        operation: "set parent to entity in different namespace".to_string(),
                    });
                }
            }
            _ => {}
        }

        Ok(())
    }

    /// Validate an entity patch
    fn validate_entity_patch(&self, patch: &crate::patch::EntityPatch, namespace: &Namespace) -> ValidationResult {
        // Check namespace can modify this entity
        if !namespace.can_modify(patch.entity.namespace) {
            return Err(ValidationError::PermissionDenied {
                namespace: namespace.id,
                operation: "modify entity in different namespace".to_string(),
            });
        }

        match &patch.op {
            EntityOp::Create { archetype, components } => {
                // Check permission
                if !namespace.permissions.create_entities {
                    return Err(ValidationError::PermissionDenied {
                        namespace: namespace.id,
                        operation: "create entities".to_string(),
                    });
                }

                // Check entity doesn't already exist
                if self.entity_exists(&patch.entity) {
                    return Err(ValidationError::EntityAlreadyExists(patch.entity));
                }

                // Validate initial components
                for (comp_type, comp_data) in components {
                    if let Some(schema) = self.component_schemas.get(comp_type) {
                        schema.validate(comp_data)?;
                    }
                }
            }
            EntityOp::Destroy => {
                // Check permission
                if !namespace.permissions.destroy_entities {
                    return Err(ValidationError::PermissionDenied {
                        namespace: namespace.id,
                        operation: "destroy entities".to_string(),
                    });
                }

                // Check entity exists
                if !self.entity_exists(&patch.entity) {
                    return Err(ValidationError::EntityNotFound(patch.entity));
                }
            }
            EntityOp::Enable | EntityOp::Disable => {
                // Check entity exists
                if !self.entity_exists(&patch.entity) {
                    return Err(ValidationError::EntityNotFound(patch.entity));
                }
            }
            EntityOp::SetParent { parent } => {
                // Check entity exists
                if !self.entity_exists(&patch.entity) {
                    return Err(ValidationError::EntityNotFound(patch.entity));
                }

                // Check parent exists if specified
                if let Some(parent) = parent {
                    if !self.entity_exists(parent) {
                        return Err(ValidationError::ParentNotFound(*parent));
                    }

                    // Check for circular hierarchy
                    if self.would_create_cycle(&patch.entity, parent) {
                        return Err(ValidationError::InvalidHierarchy {
                            child: patch.entity,
                            parent: *parent,
                        });
                    }
                }
            }
            EntityOp::AddTag { .. } | EntityOp::RemoveTag { .. } => {
                // Check entity exists
                if !self.entity_exists(&patch.entity) {
                    return Err(ValidationError::EntityNotFound(patch.entity));
                }
            }
        }

        Ok(())
    }

    /// Validate a component patch
    fn validate_component_patch(&self, patch: &crate::patch::ComponentPatch, namespace: &Namespace) -> ValidationResult {
        // Check namespace can modify this entity
        if !namespace.can_modify(patch.entity.namespace) {
            return Err(ValidationError::PermissionDenied {
                namespace: namespace.id,
                operation: "modify entity in different namespace".to_string(),
            });
        }

        // Check permission
        if !namespace.permissions.modify_components {
            return Err(ValidationError::PermissionDenied {
                namespace: namespace.id,
                operation: "modify components".to_string(),
            });
        }

        // Check entity exists
        if !self.entity_exists(&patch.entity) {
            return Err(ValidationError::EntityNotFound(patch.entity));
        }

        match &patch.op {
            ComponentOp::Set { data } => {
                // Validate against schema if registered
                if let Some(schema) = self.component_schemas.get(&patch.component) {
                    schema.validate(data)?;
                }
            }
            ComponentOp::Update { fields } => {
                // Check component exists
                if !self.component_exists(&patch.entity, &patch.component) {
                    return Err(ValidationError::ComponentNotFound {
                        entity: patch.entity,
                        component: patch.component.clone(),
                    });
                }

                // Validate fields against schema if registered
                if let Some(schema) = self.component_schemas.get(&patch.component) {
                    for (field_name, field_value) in fields {
                        if let Some(field_schema) = schema.required_fields.get(field_name)
                            .or_else(|| schema.optional_fields.get(field_name))
                        {
                            field_schema.validate(field_name, field_value)?;
                        }
                    }
                }
            }
            ComponentOp::Remove => {
                // Check component exists
                if !self.component_exists(&patch.entity, &patch.component) {
                    return Err(ValidationError::ComponentNotFound {
                        entity: patch.entity,
                        component: patch.component.clone(),
                    });
                }
            }
        }

        Ok(())
    }

    /// Validate a layer patch
    fn validate_layer_patch(&self, patch: &crate::patch::LayerPatch, namespace: &Namespace) -> ValidationResult {
        // Check permission
        if !namespace.permissions.create_layers {
            return Err(ValidationError::PermissionDenied {
                namespace: namespace.id,
                operation: "create layers".to_string(),
            });
        }

        match &patch.op {
            crate::patch::LayerOp::Create { .. } => {
                // Check layer doesn't already exist
                if self.layers.contains(&patch.layer_id) {
                    return Err(ValidationError::InvalidLayerId(patch.layer_id.clone()));
                }
            }
            crate::patch::LayerOp::Update { .. } | crate::patch::LayerOp::Destroy => {
                // Check layer exists
                if !self.layers.contains(&patch.layer_id) {
                    return Err(ValidationError::InvalidLayerId(patch.layer_id.clone()));
                }
            }
        }

        Ok(())
    }

    /// Validate an asset patch
    fn validate_asset_patch(&self, patch: &crate::patch::AssetPatch, namespace: &Namespace) -> ValidationResult {
        // Check permission
        if !namespace.permissions.load_assets {
            return Err(ValidationError::PermissionDenied {
                namespace: namespace.id,
                operation: "load assets".to_string(),
            });
        }

        match &patch.op {
            AssetOp::Load { path, .. } => {
                // Basic path validation (no parent directory traversal)
                if path.contains("..") {
                    return Err(ValidationError::InvalidAssetPath(path.clone()));
                }
            }
            AssetOp::Unload | AssetOp::Update { .. } => {
                // Asset operations are generally allowed
            }
        }

        Ok(())
    }

    /// Check if setting a parent would create a cycle
    fn would_create_cycle(&self, child: &EntityRef, new_parent: &EntityRef) -> bool {
        let mut current = *new_parent;
        let mut visited = HashSet::new();

        while let Some(parent) = self.hierarchy.get(&current) {
            if *parent == *child {
                return true; // Cycle detected
            }
            if !visited.insert(current) {
                return true; // Already visited - cycle
            }
            current = *parent;
        }

        false
    }
}

impl Default for ValidationContext {
    fn default() -> Self {
        Self::new()
    }
}

/// Validator for patches with a shared context
pub struct PatchValidator {
    context: ValidationContext,
}

impl PatchValidator {
    /// Create a new validator
    pub fn new() -> Self {
        Self {
            context: ValidationContext::new(),
        }
    }

    /// Create with a context
    pub fn with_context(context: ValidationContext) -> Self {
        Self { context }
    }

    /// Get a reference to the context
    pub fn context(&self) -> &ValidationContext {
        &self.context
    }

    /// Get a mutable reference to the context
    pub fn context_mut(&mut self) -> &mut ValidationContext {
        &mut self.context
    }

    /// Validate a single patch
    pub fn validate_patch(&self, patch: &Patch) -> ValidationResult {
        self.context.validate_patch(patch)
    }

    /// Validate multiple patches
    pub fn validate_patches(&self, patches: &[Patch]) -> ValidationResult {
        for patch in patches {
            self.validate_patch(patch)?;
        }
        Ok(())
    }
}

impl Default for PatchValidator {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::namespace::Namespace;
    use crate::patch::{EntityPatch, PatchKind};

    #[test]
    fn test_field_schema_validation() {
        let schema = FieldSchema::Int {
            min: Some(0),
            max: Some(100),
        };

        assert!(schema.validate("test", &Value::from(50)).is_ok());
        assert!(schema.validate("test", &Value::from(-1)).is_err());
        assert!(schema.validate("test", &Value::from(101)).is_err());
    }

    #[test]
    fn test_component_schema_validation() {
        let schema = ComponentSchema::new("Transform")
            .with_required_field("position", FieldSchema::Vec3)
            .with_optional_field("rotation", FieldSchema::Vec4);

        let valid_value: Value = [
            ("position", Value::from([0.0, 0.0, 0.0])),
        ]
        .into_iter()
        .collect();

        assert!(schema.validate(&valid_value).is_ok());

        let invalid_value: Value = [
            ("rotation", Value::from([0.0, 0.0, 0.0, 1.0])),
        ]
        .into_iter()
        .collect();

        // Missing required field
        assert!(schema.validate(&invalid_value).is_err());
    }

    #[test]
    fn test_validation_context() {
        let mut ctx = ValidationContext::new();
        let ns = Arc::new(Namespace::new("test"));
        let entity = EntityRef::new(ns.id, 1);

        ctx.register_namespace(ns.clone());
        ctx.register_entity(entity);

        assert!(ctx.entity_exists(&entity));
        assert!(!ctx.component_exists(&entity, "Transform"));

        ctx.register_component(entity, "Transform".to_string());
        assert!(ctx.component_exists(&entity, "Transform"));
    }

    #[test]
    fn test_patch_validation() {
        let mut ctx = ValidationContext::new();
        let ns = Arc::new(Namespace::new("test"));
        let entity = EntityRef::new(ns.id, 1);

        ctx.register_namespace(ns.clone());

        // Validate entity creation
        let patch = Patch::new(
            ns.id,
            PatchKind::Entity(EntityPatch::create(ns.id, 1)),
        );

        assert!(ctx.validate_patch(&patch).is_ok());

        // Register entity and try to create again (should fail)
        ctx.register_entity(entity);
        assert!(ctx.validate_patch(&patch).is_err());
    }
}
