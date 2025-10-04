# Font Directory

This directory should contain the `ABF.ttf` font file for the Live Text System.

## Installation

Place the `ABF.ttf` font file in this directory. The system will automatically detect and use it for both the sender and receiver applications.

## Font Path Priority

The system searches for fonts in the following order:

1. `fonts/ABF.ttf` (project directory)
2. `../fonts/ABF.ttf` (parent directory)
3. `../../fonts/ABF.ttf` (grandparent directory)
4. System fallback fonts (Arial, Helvetica, etc.)

## Font Sizes

The system uses two pre-configured font sizes:
- **Small Text**: 48px
- **Big Text**: 96px

These sizes are optimized for high-resolution displays and video projection systems.

## Note for Testing

If ABF.ttf is not available, the system will fall back to system fonts. However, for production use, please ensure ABF.ttf is properly installed in this directory.