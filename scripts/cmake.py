#!/usr/bin/env python3
import argparse
import subprocess


def main():
    parser = argparse.ArgumentParser(description="CMake helper script")
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--debug",
        action="store_const",
        dest="build_type",
        const="Debug",
        help="Use Debug build type",
    )
    group.add_argument(
        "--release",
        action="store_const",
        dest="build_type",
        const="Release",
        help="Use Release build type (default)",
    )
    parser.add_argument("-B", "--build-dir", help="Build directory")

    args, extra = parser.parse_known_args()

    build_type = args.build_type if args.build_type else "Release"

    build_dir = args.build_dir if args.build_dir else f"build/{build_type}"

    # Configure step
    configure_args = [
        "cmake",
        "-S", ".",
        "-B", build_dir,
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]

    configure_args.extend(extra)
    subprocess.check_call(configure_args)

    # Build step
    build_args = ["cmake", "--build", build_dir, "--config", build_type]
    subprocess.check_call(build_args)

    # Test step
    test_args = ["ctest", "--output-on-failure", "--test-dir", build_dir, "-C", build_type]
    subprocess.check_call(test_args)


if __name__ == "__main__":
    main()
