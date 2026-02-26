# Hello World iOS App

A simple Hello World iOS app built with Swift.

## Project Structure

```
HelloWorld/
├── AppDelegate.swift      # App lifecycle management
├── SceneDelegate.swift    # Scene lifecycle management
├── ViewController.swift   # Main view with "Hello, World!" label
├── Info.plist            # App configuration
└── project.yml           # XcodeGen configuration
```

## Requirements

- Xcode 15.0 or later
- XcodeGen (install via `brew install xcodegen`)

## How to Run

1. Generate the Xcode project:
   ```bash
   cd mobile app/ios/HelloWorld
   xcodegen generate
   ```

2. Open the project in Xcode:
   ```bash
   open HelloWorld.xcodeproj
   ```

3. Select a simulator (e.g., iPhone 15)

4. Press `Cmd + R` to build and run

## Features

- Displays "Hello, World!" in the center of the screen
- Uses UIKit with Auto Layout constraints
- Supports iOS 15.0+
