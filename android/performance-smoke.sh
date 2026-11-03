#!/usr/bin/env bash
set -euo pipefail
mkdir -p android/smoke-output
adb shell wm size 2208x1768
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 1
for variant in Debug Release; do
  root=$(find android/perf-builds -maxdepth 1 -type d -name "OpenH4-native-$variant-*")
  test -n "$root"
  lower=$(printf '%s' "$variant" | tr '[:upper:]' '[:lower:]')
  adb install -r "$root/outputs/apk/$lower/app-$lower.apk"
  adb install -r "$root/outputs/apk/androidTest/$lower/app-$lower-androidTest.apk"
  adb logcat -c
  timeout 600s adb shell am instrument -w -e performance true org.openh4.android.test/org.openh4.android.ImportSmokeTest | tee "android/smoke-output/$lower-test.txt"
  grep -q 'PASS: renderer benchmark' "android/smoke-output/$lower-test.txt"
  adb logcat -d > "android/smoke-output/$lower-logcat.txt"
  ! grep -q 'FATAL EXCEPTION' "android/smoke-output/$lower-logcat.txt"
  adb shell dumpsys meminfo org.openh4.android > "android/smoke-output/$lower-memory.txt"
  adb shell am force-stop org.openh4.android
 done
python3 android/performance_report.py android/smoke-output
