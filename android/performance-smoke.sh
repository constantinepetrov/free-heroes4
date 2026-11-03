#!/usr/bin/env bash
set -euo pipefail
# This script is restricted to the disposable GitHub Actions emulator.
test "${GITHUB_ACTIONS:-false}" = true
export ANDROID_SERIAL=emulator-5554
test "$(adb shell getprop ro.kernel.qemu | tr -d '\r')" = 1
mkdir -p android/smoke-output
lscpu > android/smoke-output/runner-cpu.txt
adb shell wm size 2208x1768
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 1
adb shell getprop > android/smoke-output/emulator-properties.txt
for label in baseline-debug baseline-release optimized-debug optimized-release; do
  renderer=${label%-*}
  lower=${label##*-}
  variant=$(printf '%s' "$lower" | sed 's/^./\U&/')
  root=$(find android/perf-builds -maxdepth 1 -type d -name "OpenH4-native-$renderer-$variant-*")
  test -n "$root"
  adb install -r "$root/outputs/apk/$lower/app-$lower.apk"
  adb install -r "$root/outputs/apk/androidTest/$lower/app-$lower-androidTest.apk"
  adb logcat -c
  timeout 600s adb shell am instrument -w -e performance true org.openh4.android.test/org.openh4.android.ImportSmokeTest | tee "android/smoke-output/$label-test.txt"
  grep -q 'PASS: renderer benchmark' "android/smoke-output/$label-test.txt"
  adb logcat -d > "android/smoke-output/$label-logcat.txt"
  ! grep -q 'FATAL EXCEPTION' "android/smoke-output/$label-logcat.txt"
  adb shell dumpsys meminfo org.openh4.android > "android/smoke-output/$label-memory.txt"
  adb shell am force-stop org.openh4.android
 done
python3 android/performance_report.py android/smoke-output
