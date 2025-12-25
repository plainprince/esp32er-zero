-- PWM Control
-- Control PWM duty cycle with +/- 10%

local freePins = gpio.freePins()
local pinIndex = 1
local dutyCycle = 0  -- 0-255

function setup()
    if #freePins == 0 then
        display.clear()
        display.println("No free pins!")
        display.println("Check display config")
        display.refresh()
        app.delay(2000)
        app.exit()
        return
    end
    
    -- Setup PWM on first pin
    pwm.setup(freePins[pinIndex], 5000, 8)  -- 5kHz, 8-bit resolution
    pwm.write(freePins[pinIndex], dutyCycle)
    render()
end

function render()
    display.clear()
    display.println("PWM Control")
    display.println("")
    local pinText = "Pin " .. pinIndex .. "/" .. #freePins .. ": GPIO" .. freePins[pinIndex]
    display.println(pinText)
    local dutyPercent = math.floor((dutyCycle / 255) * 100)
    local dutyText = "Duty: " .. dutyCycle .. " (" .. dutyPercent .. "%)"
    display.println(dutyText)
    display.println("")
    display.println("Left/Right: pin")
    display.println("Up: +10%  Down: -10%")
    display.println("Button: exit")
    display.refresh()
end

function loop()
    local btn = input.button()
    local up = input.up()
    local down = input.down()
    local left = input.left()
    local right = input.right()
    
    if btn then
        pwm.write(freePins[pinIndex], 0)  -- Turn off before exit
        app.exit()
        return
    end
    
    local changed = false
    
    if left then
        pwm.write(freePins[pinIndex], 0)  -- Turn off current pin
        pinIndex = pinIndex - 1
        if pinIndex < 1 then pinIndex = #freePins end
        pwm.setup(freePins[pinIndex], 5000, 8)
        dutyCycle = 0
        pwm.write(freePins[pinIndex], dutyCycle)
        changed = true
    end
    
    if right then
        pwm.write(freePins[pinIndex], 0)  -- Turn off current pin
        pinIndex = pinIndex + 1
        if pinIndex > #freePins then pinIndex = 1 end
        pwm.setup(freePins[pinIndex], 5000, 8)
        dutyCycle = 0
        pwm.write(freePins[pinIndex], dutyCycle)
        changed = true
    end
    
    if up then
        -- Increase by 10% (25.5 out of 255)
        dutyCycle = pwm.adjust(freePins[pinIndex], 26)  -- Rounded to 26
        changed = true
        app.delay(150)
    end
    
    if down then
        -- Decrease by 10%
        dutyCycle = pwm.adjust(freePins[pinIndex], -26)
        changed = true
        app.delay(150)
    end
    
    if changed then render() end
    app.delay(50)
end



