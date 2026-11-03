#!/usr/bin/env bash
set -euo pipefail
mkdir -p android/smoke-output
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb install -r android/app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb logcat -c
adb shell am instrument -w org.openh4.android.test/org.openh4.android.ImportSmokeTest | tee android/smoke-output/import-test.txt
grep -q 'PASS:' android/smoke-output/import-test.txt
adb shell am start -W -n org.openh4.android/.LauncherActivity
sleep 3
adb shell pidof org.openh4.android
adb shell uiautomator dump /sdcard/openh4-ui.xml
adb pull /sdcard/openh4-ui.xml android/smoke-output/
adb shell screencap -p /sdcard/openh4.png
adb pull /sdcard/openh4.png android/smoke-output/
adb logcat -d > android/smoke-output/logcat.txt
! grep -q 'FATAL EXCEPTION' android/smoke-output/logcat.txt
