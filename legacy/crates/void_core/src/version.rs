//! Semantic versioning for hot-reload compatibility

use core::fmt;
use core::cmp::Ordering;

/// Semantic version for compatibility checking
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub struct Version {
    pub major: u16,
    pub minor: u16,
    pub patch: u16,
}

impl Version {
    /// Create a new version
    #[inline]
    pub const fn new(major: u16, minor: u16, patch: u16) -> Self {
        Self { major, minor, patch }
    }

    /// Version 0.0.0
    pub const ZERO: Version = Version::new(0, 0, 0);

    /// Check if this version is compatible with another (same major, >= minor)
    pub fn is_compatible_with(&self, other: &Version) -> bool {
        if self.major == 0 && other.major == 0 {
            // Pre-1.0: minor version must match exactly
            self.minor == other.minor
        } else {
            // Post-1.0: major must match, self must be >= other
            self.major == other.major &&
            (self.minor > other.minor ||
             (self.minor == other.minor && self.patch >= other.patch))
        }
    }

    /// Parse from string "major.minor.patch"
    pub fn parse(s: &str) -> Option<Self> {
        let mut parts = s.split('.');
        let major = parts.next()?.parse().ok()?;
        let minor = parts.next()?.parse().ok()?;
        let patch = parts.next()?.parse().ok()?;
        Some(Self { major, minor, patch })
    }

    /// Convert to a single u64 for easy comparison
    #[inline]
    pub const fn to_u64(&self) -> u64 {
        (self.major as u64) << 32 | (self.minor as u64) << 16 | self.patch as u64
    }
}

impl PartialOrd for Version {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Version {
    fn cmp(&self, other: &Self) -> Ordering {
        self.to_u64().cmp(&other.to_u64())
    }
}

impl fmt::Display for Version {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{}.{}", self.major, self.minor, self.patch)
    }
}

impl fmt::Debug for Version {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Version({})", self)
    }
}

impl Default for Version {
    fn default() -> Self {
        Self::new(0, 1, 0)
    }
}

/// Macro for creating versions at compile time
#[macro_export]
macro_rules! version {
    ($major:literal . $minor:literal . $patch:literal) => {
        $crate::Version::new($major, $minor, $patch)
    };
    ($major:literal . $minor:literal) => {
        $crate::Version::new($major, $minor, 0)
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_version_parsing() {
        let v = Version::parse("1.2.3").unwrap();
        assert_eq!(v.major, 1);
        assert_eq!(v.minor, 2);
        assert_eq!(v.patch, 3);
    }

    #[test]
    fn test_version_compatibility() {
        let v1 = Version::new(1, 2, 0);
        let v2 = Version::new(1, 2, 1);
        let v3 = Version::new(1, 3, 0);
        let v4 = Version::new(2, 0, 0);

        assert!(v2.is_compatible_with(&v1)); // 1.2.1 is compatible with 1.2.0
        assert!(v3.is_compatible_with(&v1)); // 1.3.0 is compatible with 1.2.0
        assert!(!v1.is_compatible_with(&v2)); // 1.2.0 is NOT compatible with 1.2.1
        assert!(!v4.is_compatible_with(&v1)); // 2.0.0 is NOT compatible with 1.x
    }

    #[test]
    fn test_version_ordering() {
        let v1 = Version::new(1, 0, 0);
        let v2 = Version::new(1, 1, 0);
        let v3 = Version::new(2, 0, 0);

        assert!(v1 < v2);
        assert!(v2 < v3);
        assert!(v1 < v3);
    }
}
