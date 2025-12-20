


local holdStart = 0

function setup()
    render()
end

function render()
    local x = input.joystickX()
    local y = input.joystickY()
    local btn = input.buttonRaw()
    
    display.clear()
    display.println("Joystick Test")
    
    
    display.print(0, 15, "X: " .. tostring(x))
    display.print(0, 27, "Y: " .. tostring(y))
    display.print(0, 39, "Btn: " .. (btn and "PRESSED" or "released"))
    display.print(0, 56, "Hold btn to exit")
    
    
    local w = display.width()
    local h = display.height()
    
    
    
    
    local boxW = math.floor(w / 3)
    local boxH = math.floor(h / 3)
    
    
    local boxX = math.floor((w * 3 / 4) - (boxW / 2))
    local boxY = math.floor((h * 3 / 4) - (boxH / 2))
    
    
    display.drawRect(boxX, boxY, boxW, boxH)
    
    
    local invX = 4095 - x
    local invY = 4095 - y
    
    local cx = boxX + math.floor((invX / 4095) * boxW)
    local cy = boxY + math.floor((invY / 4095) * boxH)
    
    
    display.fillRect(cx - 2, cy, 5, 1)
    display.fillRect(cx, cy - 2, 1, 5)
    
    display.refresh()
end

function loop()
    
    if input.buttonRaw() then
        if holdStart == 0 then
            holdStart = app.millis()
        elseif app.millis() - holdStart > 500 then
            app.exit()
            return
        end
    else
        holdStart = 0
    end
    
    render()
    app.delay(50)
end
