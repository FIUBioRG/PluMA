//! PluMA Rust Plugin Interface
//!
//! This module provides the interface for writing PluMA plugins in Rust.
//! It integrates with the `pluma-plugin-trait` crate and provides FFI exports
//! for the PluMA plugin system.
//!
//! # Example Plugin
//!
//! ```rust
//! use pluma_plugin_trait::PlumaPlugin;
//! use pluma_io::PluginManager;
//!
//! pub struct MyPlugin {
//!     data: String,
//! }
//!
//! impl PlumaPlugin for MyPlugin {
//!     fn new() -> Self {
//!         MyPlugin { data: String::new() }
//!     }
//!
//!     fn input(&mut self, filename: &str) {
//!         // Read input data
//!         self.data = std::fs::read_to_string(filename).unwrap_or_default();
//!     }
//!
//!     fn run(&mut self) {
//!         // Process data
//!         self.data = self.data.to_uppercase();
//!     }
//!
//!     fn output(&mut self, filename: &str) {
//!         // Write output data
//!         std::fs::write(filename, &self.data).ok();
//!     }
//! }
//!
//! // Export the plugin for PluMA
//! pluma_plugin_trait::export_plugin!(MyPlugin);
//! ```

use std::ffi::CStr;
use std::os::raw::c_char;

/// PluginManager provides access to PluMA runtime functions.
/// This mirrors the C++ PluginManager interface.
pub struct PluginManager;

impl PluginManager {
    /// Log a message to the PluMA log file
    pub fn log(msg: &str) {
        eprintln!("[PluMA/Rust] {}", msg);
    }

    /// Check if a plugin dependency is installed
    pub fn dependency(plugin: &str) {
        Self::log(&format!("Checking dependency: {}", plugin));
    }

    /// Get the current prefix path
    pub fn prefix() -> String {
        std::env::var("PLUMA_PREFIX").unwrap_or_else(|_| String::from(""))
    }

    /// Add prefix to a filename
    pub fn add_prefix(filename: &str) -> String {
        format!("{}/{}", Self::prefix(), filename)
    }
}

/// Convert a C string pointer to a Rust String
///
/// # Safety
/// The pointer must be a valid null-terminated C string
pub unsafe fn c_str_to_string(ptr: *const c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }
    CStr::from_ptr(ptr)
        .to_str()
        .unwrap_or("")
        .to_string()
}

/// Macro to generate FFI exports for a PluMA plugin
///
/// This macro generates the necessary C-compatible functions that PluMA
/// uses to load and execute Rust plugins.
///
/// # Usage
/// ```rust
/// use pluma_plugin_trait::PlumaPlugin;
///
/// struct MyPlugin { /* ... */ }
///
/// impl PlumaPlugin for MyPlugin {
///     // ... implementation
/// }
///
/// // Generate FFI exports
/// pluma_export_plugin!(MyPlugin);
/// ```
#[macro_export]
macro_rules! pluma_export_plugin {
    ($plugin_type:ty) => {
        /// Create a new plugin instance
        #[no_mangle]
        pub extern "C" fn plugin_create() -> *mut std::ffi::c_void {
            let plugin = Box::new(<$plugin_type as pluma_plugin_trait::PlumaPlugin>::new());
            Box::into_raw(plugin) as *mut std::ffi::c_void
        }

        /// Destroy a plugin instance
        #[no_mangle]
        pub extern "C" fn plugin_destroy(ptr: *mut std::ffi::c_void) {
            if !ptr.is_null() {
                unsafe {
                    let _ = Box::from_raw(ptr as *mut $plugin_type);
                }
            }
        }

        /// Execute the input phase
        #[no_mangle]
        pub extern "C" fn plugin_input(ptr: *mut std::ffi::c_void, filename: *const std::os::raw::c_char) {
            if ptr.is_null() || filename.is_null() {
                return;
            }
            unsafe {
                let plugin = &mut *(ptr as *mut $plugin_type);
                let filename_str = std::ffi::CStr::from_ptr(filename)
                    .to_str()
                    .unwrap_or("");
                plugin.input(filename_str);
            }
        }

        /// Execute the run phase
        #[no_mangle]
        pub extern "C" fn plugin_run(ptr: *mut std::ffi::c_void) {
            if ptr.is_null() {
                return;
            }
            unsafe {
                let plugin = &mut *(ptr as *mut $plugin_type);
                plugin.run();
            }
        }

        /// Execute the output phase
        #[no_mangle]
        pub extern "C" fn plugin_output(ptr: *mut std::ffi::c_void, filename: *const std::os::raw::c_char) {
            if ptr.is_null() || filename.is_null() {
                return;
            }
            unsafe {
                let plugin = &mut *(ptr as *mut $plugin_type);
                let filename_str = std::ffi::CStr::from_ptr(filename)
                    .to_str()
                    .unwrap_or("");
                plugin.output(filename_str);
            }
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_c_str_conversion() {
        let test_str = std::ffi::CString::new("test").unwrap();
        unsafe {
            let result = c_str_to_string(test_str.as_ptr());
            assert_eq!(result, "test");
        }
    }

    #[test]
    fn test_null_c_str() {
        unsafe {
            let result = c_str_to_string(std::ptr::null());
            assert_eq!(result, "");
        }
    }
}
