# Lua API Documentation

## Display Module

### display.clear()
Clears the display buffer.

### display.print(x, y, text)
Prints text at the specified coordinates.
- `x`: X coordinate
- `y`: Y coordinate  
- `text`: Text to print

### display.println(text)
Prints text at current cursor position and moves to next line.
- `text`: Text to print

### display.drawRect(x, y, w, h)
Draws a rectangle outline.
- `x`, `y`: Top-left corner
- `w`, `h`: Width and height

### display.fillRect(x, y, w, h)
Fills a rectangle.
- `x`, `y`: Top-left corner
- `w`, `h`: Width and height

### display.drawPixel(x, y)
Draws a single pixel.
- `x`, `y`: Pixel coordinates

### display.refresh()
Queues a display refresh request (non-blocking).

### display.width()
Returns the display width in pixels.

### display.height()
Returns the display height in pixels.

### display.textScale()
Returns the current text scale factor.

### display.textHeight()
Returns the raw text height in pixels.

## Input Module

### input.button()
Returns true if button was just released (edge trigger).

### input.up()
Returns true if joystick up was just pressed.

### input.down()
Returns true if joystick down was just pressed.

### input.left()
Returns true if joystick left was just pressed.

### input.right()
Returns true if joystick right was just pressed.

### input.joystickX()
Returns raw joystick X value (0-4095).

### input.joystickY()
Returns raw joystick Y value (0-4095).

### input.buttonRaw()
Returns current button state (true = pressed).

## App Module

### app.exit()
Exits the current application.

### app.delay(ms)
Delays execution for specified milliseconds.
- `ms`: Milliseconds to delay

### app.millis()
Returns milliseconds since boot.

## GPIO Module

### gpio.mode(pin, mode)
Sets GPIO pin mode.
- `pin`: Pin number
- `mode`: "output", "input", "input_pullup", or "input_pulldown"

### gpio.write(pin, value)
Writes digital value to pin.
- `pin`: Pin number
- `value`: true = HIGH, false = LOW

### gpio.read(pin)
Reads digital value from pin.
- `pin`: Pin number
- Returns: true = HIGH, false = LOW

### gpio.analogRead(pin)
Reads analog value from pin (0-4095).
- `pin`: Pin number

### gpio.analogWrite(pin, value)
Writes PWM value to pin.
- `pin`: Pin number
- `value`: PWM value

## HTTP Module

### http.post(url, body, headers)
Sends HTTP POST request.
- `url`: Request URL
- `body`: Request body (JSON string)
- `headers`: Optional Lua table with header key-value pairs
- Returns: `status_code, response_body`

Example:
```lua
local headers = {
    ["Authorization"] = "Bearer token123"
}
local code, res = http.post("https://api.example.com/endpoint", '{"key":"value"}', headers)
```

## GUI Module

### gui.keyboard(title, initial)
Shows on-screen keyboard and returns entered text.
- `title`: Keyboard title
- `initial`: Initial text value (optional)
- Returns: Entered text string, or nil if cancelled

### gui.drawInput(x, y, w, text, focused, label, minInputWidth)
Draws an input field.
- `x`, `y`: Position
- `w`: Width (total width including label if provided)
- `text`: Current text value
- `focused`: true if field is focused
- `label`: Optional label text (displayed to the left of the input field)
- `minInputWidth`: Optional minimum width for the input field itself in scaled pixels (default: 40 * scale, relative to 128x64 base)

### gui.getInputYBelow(x, y, w, text, label)
Returns the Y coordinate below an input field (for positioning next field).
- `x`, `y`: Position of the input field
- `w`: Width of the input field
- `text`: Text value (used to calculate height)
- `label`: Optional label (for consistency with drawInput)
- Returns: Y coordinate below the input field

### gui.getInputBottomY(x, y, w, text, label, focused)
Returns the bottom Y coordinate of an input field including border.
- `x`, `y`: Position of the input field
- `w`: Width of the input field
- `text`: Text value (used to calculate height)
- `label`: Optional label
- `focused`: true if field is focused (affects border thickness: 2px when focused, 1px when not)
- Returns: Bottom Y coordinate of the input field including border

### gui.getInputRightX(x, y, w, text, label, minInputWidth)
Returns the right X coordinate of an input field (accounting for label and minInputWidth).
- `x`, `y`: Position of the input field
- `w`: Width of the input field
- `text`: Text value
- `label`: Optional label
- `minInputWidth`: Optional minimum input width (default: 40 * scale)
- Returns: Right X coordinate of the entire field (label + input)

### gui.getInputLeftX(x, y, w, text, label, minInputWidth)
Returns the left X coordinate of the input field itself (after the label).
- `x`, `y`: Position of the input field
- `w`: Width of the input field
- `text`: Text value
- `label`: Optional label
- `minInputWidth`: Optional minimum input width (default: 40 * scale)
- Returns: Left X coordinate of the input field (after label)

### gui.getInputTopY(x, y, w, text, label)
Returns the top Y coordinate of an input field (same as y parameter).
- `x`, `y`: Position of the input field
- `w`: Width of the input field
- `text`: Text value
- `label`: Optional label
- Returns: Top Y coordinate (same as y)

### gui.drawButton(x, y, text, focused, clicked)
Draws a button. Button size is automatically calculated based on text content.
- `x`, `y`: Top-left corner position
- `text`: Button text (button width and height are auto-calculated from text)
- `focused`: true if button is focused
- `clicked`: true if button is currently clicked
- If text is too long (remaining display space <= 128 scaled pixels), text is truncated and an ellipsis (...) icon is shown

### gui.getButtonWidth(text, x)
Returns the width of a button with the given text at the given x position.
- `text`: Button text
- `x`: Optional x position (for truncation calculation)
- Returns: Button width in pixels

### gui.getButtonBottomY(y, text, focused, x)
Returns the bottom Y coordinate of a button including border.
- `y`: Top Y position of the button
- `text`: Button text
- `focused`: Optional, true if button is focused (affects border thickness: 2px when focused, 1px when not)
- `x`: Optional x position (for truncation calculation)
- Returns: Bottom Y coordinate of the button including border

### gui.createApp(title, items)
Creates an interactive application with input fields and buttons. Handles navigation, selection, scrolling, and rendering automatically.
- `title`: Application title (displayed in header)
- `items`: Table of items, each item is a table with:
  - `type`: "input" or "button"
  - `label`: Label text (for inputs) or button text (for buttons)
  - `value`: Initial value string (for inputs only, optional)
  - `callback`: Lua function to call when button is pressed (for buttons only, optional)
  - `x`: X position (optional, default: 0)
  - `w`: Width (optional, default: 128 for inputs, auto for buttons)
  - `minInputWidth`: Minimum input width (for inputs only, optional, default: 40 * scale)
- Returns: Table with functions:
  - `update()`: Call this in your loop() function to handle input and render (rendering is built-in)
  - `exit()`: Call this to clean up and exit the app
  - `getInputValue(index)`: Get the current value of an input field (1-based index)

Example:
```lua
local app = gui.createApp("My App", {
    {type="input", label="Name", value="", x=0, w=128},
    {type="input", label="Email", value="", x=0, w=128},
    {type="button", label="Submit", callback=function() 
        -- Access input values via app.getInputValue(index)
        local name = app.getInputValue(1)
        local email = app.getInputValue(2)
        print("Submitted: " .. name .. ", " .. email)
        app.exit()
    end}
})

function loop()
    app.update()  -- Handles input and rendering automatically
    app.delay(10)
end
```

### gui.showLoadingScreen()
Shows the loading screen overlay (drawn on top of current buffer).

### gui.hideLoadingScreen()
Hides the loading screen overlay.

### gui.isLoadingScreenVisible()
Returns true if loading screen is currently visible.
