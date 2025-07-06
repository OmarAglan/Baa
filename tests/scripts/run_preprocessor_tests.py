#!/usr/bin/env python3
"""
Comprehensive Preprocessor Test Runner
=====================================

This script runs all preprocessor tests and provides detailed reporting.
It's designed to be used as part of the Baa compiler testing strategy.
"""

import os
import sys
import subprocess
import time
from pathlib import Path

class TestRunner:
    def __init__(self, build_dir="build"):
        self.build_dir = Path(build_dir)
        self.test_results = []
        self.total_tests = 0
        self.passed_tests = 0
        self.failed_tests = 0
        
    def run_test(self, test_name, test_executable):
        """Run a single test and capture results"""
        print(f"\n{'='*60}")
        print(f"Running: {test_name}")
        print(f"{'='*60}")
        
        test_path = self.build_dir / "tests" / "unit" / "preprocessor" / test_executable
        
        if not test_path.exists():
            print(f"❌ Test executable not found: {test_path}")
            self.test_results.append({
                'name': test_name,
                'status': 'NOT_FOUND',
                'duration': 0,
                'output': f"Executable not found: {test_path}"
            })
            self.failed_tests += 1
            return False
            
        start_time = time.time()
        
        try:
            # Run the test
            result = subprocess.run(
                [str(test_path)],
                capture_output=True,
                text=True,
                encoding='utf-8',
                timeout=30  # 30 second timeout
            )
            
            duration = time.time() - start_time
            
            if result.returncode == 0:
                print(f"✅ {test_name} PASSED ({duration:.2f}s)")
                self.test_results.append({
                    'name': test_name,
                    'status': 'PASSED',
                    'duration': duration,
                    'output': result.stdout
                })
                self.passed_tests += 1
                return True
            else:
                print(f"❌ {test_name} FAILED ({duration:.2f}s)")
                print(f"Return code: {result.returncode}")
                if result.stdout:
                    print("STDOUT:")
                    print(result.stdout)
                if result.stderr:
                    print("STDERR:")
                    print(result.stderr)
                    
                self.test_results.append({
                    'name': test_name,
                    'status': 'FAILED',
                    'duration': duration,
                    'output': result.stdout,
                    'error': result.stderr,
                    'return_code': result.returncode
                })
                self.failed_tests += 1
                return False
                
        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            print(f"⏰ {test_name} TIMEOUT ({duration:.2f}s)")
            self.test_results.append({
                'name': test_name,
                'status': 'TIMEOUT',
                'duration': duration,
                'output': "Test timed out after 30 seconds"
            })
            self.failed_tests += 1
            return False
            
        except Exception as e:
            duration = time.time() - start_time
            print(f"💥 {test_name} ERROR ({duration:.2f}s): {e}")
            self.test_results.append({
                'name': test_name,
                'status': 'ERROR',
                'duration': duration,
                'output': f"Exception: {e}"
            })
            self.failed_tests += 1
            return False
    
    def run_all_preprocessor_tests(self):
        """Run all preprocessor tests in order"""
        
        # Define all preprocessor tests in logical order
        tests = [
            ("Basic Preprocessor Conditionals", "test_preprocessor"),
            ("Preprocessor Conditionals", "test_preprocessor_conditionals"),
            ("Basic Macro Functionality", "test_preprocessor_macros"),
            ("Predefined Macros", "test_preprocessor_predefined"),
            ("Error and Warning Directives", "test_preprocessor_directives"),
            ("Enhanced Error System", "test_enhanced_error_system"),
            ("Advanced Macro Features", "test_preprocessor_advanced_macros"),
            ("Advanced Expression Evaluation", "test_preprocessor_expressions"),
            ("Comprehensive File-Based Tests", "test_preprocessor_comprehensive"),
        ]
        
        print("🚀 Starting Comprehensive Preprocessor Test Suite")
        print(f"Build directory: {self.build_dir.absolute()}")
        print(f"Total tests to run: {len(tests)}")
        
        self.total_tests = len(tests)
        
        # Run each test
        for test_name, test_executable in tests:
            self.run_test(test_name, test_executable)
        
        # Print summary
        self.print_summary()
        
        return self.failed_tests == 0
    
    def print_summary(self):
        """Print comprehensive test summary"""
        print(f"\n{'='*80}")
        print("PREPROCESSOR TEST SUITE SUMMARY")
        print(f"{'='*80}")
        
        total_duration = sum(result['duration'] for result in self.test_results)
        
        print(f"Total Tests: {self.total_tests}")
        print(f"Passed: {self.passed_tests} ✅")
        print(f"Failed: {self.failed_tests} ❌")
        print(f"Success Rate: {(self.passed_tests/self.total_tests*100):.1f}%")
        print(f"Total Duration: {total_duration:.2f}s")
        
        # Detailed results
        print(f"\n{'='*80}")
        print("DETAILED RESULTS")
        print(f"{'='*80}")
        
        for result in self.test_results:
            status_icon = {
                'PASSED': '✅',
                'FAILED': '❌',
                'TIMEOUT': '⏰',
                'ERROR': '💥',
                'NOT_FOUND': '❓'
            }.get(result['status'], '❓')
            
            print(f"{status_icon} {result['name']:<40} {result['status']:<10} ({result['duration']:.2f}s)")
        
        # Failed test details
        failed_results = [r for r in self.test_results if r['status'] != 'PASSED']
        if failed_results:
            print(f"\n{'='*80}")
            print("FAILED TEST DETAILS")
            print(f"{'='*80}")
            
            for result in failed_results:
                print(f"\n❌ {result['name']}")
                print(f"Status: {result['status']}")
                print(f"Duration: {result['duration']:.2f}s")
                if 'return_code' in result:
                    print(f"Return Code: {result['return_code']}")
                if result.get('output'):
                    print("Output:")
                    print(result['output'])
                if result.get('error'):
                    print("Error:")
                    print(result['error'])
                print("-" * 40)
        
        # Recommendations
        print(f"\n{'='*80}")
        print("RECOMMENDATIONS")
        print(f"{'='*80}")
        
        if self.failed_tests == 0:
            print("🎉 All preprocessor tests passed! The preprocessor is ready for production.")
            print("✅ You can proceed to the next compiler component (Lexer).")
        else:
            print(f"⚠️  {self.failed_tests} test(s) failed. Address these issues before proceeding:")
            print("1. Review failed test output above")
            print("2. Fix any implementation issues")
            print("3. Re-run tests to verify fixes")
            print("4. Only proceed to next component when all tests pass")

def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(description="Run comprehensive preprocessor tests")
    parser.add_argument("--build-dir", default="build", 
                       help="Build directory containing test executables (default: build)")
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Enable verbose output")
    
    args = parser.parse_args()
    
    if args.verbose:
        print("Verbose mode enabled")
    
    # Check if build directory exists
    build_path = Path(args.build_dir)
    if not build_path.exists():
        print(f"❌ Build directory not found: {build_path.absolute()}")
        print("Please build the project first using CMake:")
        print("  mkdir build && cd build")
        print("  cmake .. && make")
        return 1
    
    # Run tests
    runner = TestRunner(args.build_dir)
    success = runner.run_all_preprocessor_tests()
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
