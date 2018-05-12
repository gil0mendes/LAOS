#![no_std]
#![no_main]
#![feature(custom_attribute)]
#![feature(allocator_api)]

#[macro_use]
extern crate bitflags;

pub mod types;
pub mod system_table;
// pub mod boot_services;

// mod memory_services;
// mod miscellaneous;

// pub use self::memory_services::types::*;
// pub use self::memory_services::{allocate_pages, free_pages, MemoryMap, MemoryMapFailure,
//                                 get_memory_map};
// pub use self::miscellaneous::set_watchdog_timer;
