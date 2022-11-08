use chrono::prelude::*;
use std::process::Command;
use std::env;

pub fn main() {
    let mut build = cc::Build::new();
    build.warnings(false);
    let arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
    let os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    build.define(&format!("CFG_TARGET_OS_{}", os), None);
    build.define(&format!("CFG_TARGET_ARCH_{}", arch), None);
    let files = ["helper.c", "lre_exec_backtrack.c"];
    for f in files {
        build.file("../../wasm_qjs_helper/helper/".to_string() + f);
        println!("{}", "cargo:rerun-if-changed=../../wasm_qjs_helper/helper/".to_string() + f);
    }
    println!("{}", "cargo:rerun-if-changed=../../wasm_qjs_helper/helper/".to_string() + "helper.h");
    println!("{}", "cargo:rerun-if-changed=../../wasm_qjs_helper/helper/".to_string() + "quickjs.h");
    println!("{}", "cargo:rerun-if-changed=../../wasm_qjs_helper/helper/".to_string() + "lre_exec_backtrack.h");
    build.include("../../wasm_qjs_helper/helper/");
    build.compile("my-helpers");


    // Set WASMER_GIT_HASH
    let git_hash = Command::new("git")
        .args(&["rev-parse", "HEAD"])
        .output()
        .ok()
        .and_then(|output| String::from_utf8(output.stdout).ok())
        .unwrap_or_default();
    println!("cargo:rustc-env=WASMER_BUILD_GIT_HASH={}", git_hash);

    if git_hash.len() > 5 {
        println!(
            "cargo:rustc-env=WASMER_BUILD_GIT_HASH_SHORT={}",
            &git_hash[..5]
        );
    } else {
        println!("cargo:rustc-env=WASMER_BUILD_GIT_HASH_SHORT=??????");
    }

    let utc: DateTime<Utc> = Utc::now();
    let date = utc.format("%Y-%m-%d").to_string();
    println!("cargo:rustc-env=WASMER_BUILD_DATE={}", date);
}
