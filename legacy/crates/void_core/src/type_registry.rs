//! Dynamic type registry for runtime type information
//!
//! Enables dynamic component and system registration at runtime,
//! which is essential for hot-reload and plugin extensibility.

use core::any::{Any, TypeId};
use core::fmt;
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;

/// Information about a registered type
#[derive(Clone)]
pub struct TypeInfo {
    /// The Rust TypeId
    pub type_id: TypeId,
    /// Human-readable type name
    pub name: String,
    /// Size in bytes
    pub size: usize,
    /// Alignment requirement
    pub align: usize,
    /// Whether this type needs drop
    pub needs_drop: bool,
    /// Optional schema for serialization
    pub schema: Option<TypeSchema>,
}

impl TypeInfo {
    /// Create type info for a concrete type
    pub fn of<T: 'static>() -> Self {
        Self {
            type_id: TypeId::of::<T>(),
            name: core::any::type_name::<T>().into(),
            size: core::mem::size_of::<T>(),
            align: core::mem::align_of::<T>(),
            needs_drop: core::mem::needs_drop::<T>(),
            schema: None,
        }
    }

    /// Create with a schema
    pub fn with_schema(mut self, schema: TypeSchema) -> Self {
        self.schema = Some(schema);
        self
    }
}

impl fmt::Debug for TypeInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("TypeInfo")
            .field("name", &self.name)
            .field("size", &self.size)
            .field("align", &self.align)
            .field("needs_drop", &self.needs_drop)
            .finish()
    }
}

/// Schema describing a type's structure for serialization
#[derive(Clone, Debug)]
pub enum TypeSchema {
    /// Primitive type
    Primitive(PrimitiveType),
    /// Struct with named fields
    Struct { fields: Vec<FieldInfo> },
    /// Enum with variants
    Enum { variants: Vec<VariantInfo> },
    /// Array/Vec of another type
    Array { element: Box<TypeSchema> },
    /// Optional value
    Option { inner: Box<TypeSchema> },
    /// Map from key to value
    Map { key: Box<TypeSchema>, value: Box<TypeSchema> },
    /// Tuple of types
    Tuple { elements: Vec<TypeSchema> },
    /// Custom/opaque type
    Opaque,
}

/// Primitive types
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PrimitiveType {
    Bool,
    I8, I16, I32, I64, I128,
    U8, U16, U32, U64, U128,
    F32, F64,
    Char,
    String,
}

/// Information about a struct field
#[derive(Clone, Debug)]
pub struct FieldInfo {
    pub name: String,
    pub offset: usize,
    pub schema: TypeSchema,
}

/// Information about an enum variant
#[derive(Clone, Debug)]
pub struct VariantInfo {
    pub name: String,
    pub discriminant: i64,
    pub fields: Vec<FieldInfo>,
}

/// Central registry for all types in the engine
pub struct TypeRegistry {
    /// Map from TypeId to TypeInfo
    by_id: BTreeMap<TypeId, TypeInfo>,
    /// Map from name to TypeId for name-based lookups
    by_name: BTreeMap<String, TypeId>,
    /// Registered type constructors
    constructors: BTreeMap<TypeId, Box<dyn Fn() -> Box<dyn DynType> + Send + Sync>>,
}

impl TypeRegistry {
    /// Create a new empty registry
    pub fn new() -> Self {
        Self {
            by_id: BTreeMap::new(),
            by_name: BTreeMap::new(),
            constructors: BTreeMap::new(),
        }
    }

    /// Register a type
    pub fn register<T: DynType + Default + 'static>(&mut self) -> &mut Self {
        let info = TypeInfo::of::<T>();
        let type_id = info.type_id;
        let name = info.name.clone();

        self.by_id.insert(type_id, info);
        self.by_name.insert(name, type_id);
        self.constructors.insert(type_id, Box::new(|| Box::new(T::default())));
        self
    }

    /// Register a type with custom info
    pub fn register_with_info(&mut self, info: TypeInfo) -> &mut Self {
        let type_id = info.type_id;
        let name = info.name.clone();
        self.by_id.insert(type_id, info);
        self.by_name.insert(name, type_id);
        self
    }

    /// Get type info by TypeId
    pub fn get(&self, type_id: TypeId) -> Option<&TypeInfo> {
        self.by_id.get(&type_id)
    }

    /// Get type info by name
    pub fn get_by_name(&self, name: &str) -> Option<&TypeInfo> {
        self.by_name.get(name).and_then(|id| self.by_id.get(id))
    }

    /// Check if a type is registered
    pub fn contains(&self, type_id: TypeId) -> bool {
        self.by_id.contains_key(&type_id)
    }

    /// Check if a type is registered by name
    pub fn contains_name(&self, name: &str) -> bool {
        self.by_name.contains_key(name)
    }

    /// Create an instance of a registered type
    pub fn create(&self, type_id: TypeId) -> Option<Box<dyn DynType>> {
        self.constructors.get(&type_id).map(|f| f())
    }

    /// Create an instance by name
    pub fn create_by_name(&self, name: &str) -> Option<Box<dyn DynType>> {
        self.by_name.get(name).and_then(|id| self.create(*id))
    }

    /// Iterate over all registered types
    pub fn iter(&self) -> impl Iterator<Item = &TypeInfo> {
        self.by_id.values()
    }

    /// Get the number of registered types
    pub fn len(&self) -> usize {
        self.by_id.len()
    }

    /// Check if empty
    pub fn is_empty(&self) -> bool {
        self.by_id.is_empty()
    }
}

impl Default for TypeRegistry {
    fn default() -> Self {
        Self::new()
    }
}

impl fmt::Debug for TypeRegistry {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("TypeRegistry")
            .field("types", &self.by_id.len())
            .finish()
    }
}

/// Trait for types that can be dynamically manipulated
pub trait DynType: Any + Send + Sync {
    /// Get the type info
    fn type_info(&self) -> TypeInfo;

    /// Clone into a boxed trait object
    fn clone_box(&self) -> Box<dyn DynType>;

    /// Get as Any reference (for downcasting)
    fn as_any(&self) -> &dyn Any;

    /// Get as mutable Any reference (for downcasting)
    fn as_any_mut(&mut self) -> &mut dyn Any;

    /// Serialize to bytes (if supported)
    fn to_bytes(&self) -> Option<Vec<u8>> {
        None
    }

    /// Deserialize from bytes (if supported)
    fn from_bytes(&mut self, _bytes: &[u8]) -> bool {
        false
    }
}

impl dyn DynType {
    /// Downcast to a concrete type
    pub fn downcast_ref<T: 'static>(&self) -> Option<&T> {
        self.as_any().downcast_ref()
    }

    /// Downcast to a mutable concrete type
    pub fn downcast_mut<T: 'static>(&mut self) -> Option<&mut T> {
        self.as_any_mut().downcast_mut()
    }
}

/// Blanket implementation for common types
impl<T: Clone + Default + Send + Sync + 'static> DynType for T {
    fn type_info(&self) -> TypeInfo {
        TypeInfo::of::<T>()
    }

    fn clone_box(&self) -> Box<dyn DynType> {
        Box::new(self.clone())
    }

    fn as_any(&self) -> &dyn Any {
        self
    }

    fn as_any_mut(&mut self) -> &mut dyn Any {
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Clone, Default)]
    struct TestComponent {
        value: i32,
    }

    #[test]
    fn test_type_registry() {
        let mut registry = TypeRegistry::new();
        registry.register::<TestComponent>();

        assert!(registry.contains(TypeId::of::<TestComponent>()));

        let info = registry.get(TypeId::of::<TestComponent>()).unwrap();
        assert!(info.name.contains("TestComponent"));
    }

    #[test]
    fn test_type_info() {
        let info = TypeInfo::of::<i32>();
        assert_eq!(info.size, 4);
        assert_eq!(info.align, 4);
        assert!(!info.needs_drop);
    }
}
