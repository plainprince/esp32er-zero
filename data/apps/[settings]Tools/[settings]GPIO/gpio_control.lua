



local freePins = gpio.freePins()
local pinIndex = 1
local pinState = false

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
    gpio.mode(freePins[pinIndex], "OUTPUT")
    render()
end

function render()
    display.clear()
    display.println("GPIO Control")
    display.println("")
    local pinText = "Pin " .. pinIndex .. "/" .. #freePins .. ": GPIO" .. freePins[pinIndex]
    display.println(pinText)
    local stateText = "State: " .. (pinState and "HIGH" or "LOW")
    display.println(stateText)
    display.println("")
    display.println("Left/Right: pin")
    display.println("Up: HIGH  Down: LOW")
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
        gpio.mode(freePins[pinIndex], "INPUT")
        app.exit()
        return
    end
    
    local changed = false
    
    if left then
        gpio.mode(freePins[pinIndex], "INPUT")
        pinIndex = pinIndex - 1
        if pinIndex < 1 then pinIndex = #freePins end
        gpio.mode(freePins[pinIndex], "OUTPUT")
        pinState = false
        gpio.write(freePins[pinIndex], false)
        changed = true
    end
    
    if right then
        gpio.mode(freePins[pinIndex], "INPUT")
        pinIndex = pinIndex + 1
        if pinIndex > #freePins then pinIndex = 1 end
        gpio.mode(freePins[pinIndex], "OUTPUT")
        pinState = false
        gpio.write(freePins[pinIndex], false)
        changed = true
    end
    
    if up and not pinState then
        pinState = true
        gpio.write(freePins[pinIndex], true)
        changed = true
    end
    
    if down and pinState then
        pinState = false
        gpio.write(freePins[pinIndex], false)
        changed = true
    end
    
    if changed then render() end
    app.delay(50)
end
