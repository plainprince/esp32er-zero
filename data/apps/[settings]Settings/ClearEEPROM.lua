local confirmed = false
local selected = 1
local MAX_ITEMS = 2

function setup()
    draw_ui()
end

function draw_ui()
    display.lock()
    display.clear()
    
    if confirmed then
        display.println("Clearing EEPROM...")
        display.println("")
        display.println("Please wait...")
        display.unlock()
        display.refresh()
        
        local success = eeprom.clear()
        
        display.lock()
        display.clear()
        if success then
            display.println("EEPROM cleared")
            display.println("successfully!")
        else
            display.println("Error clearing")
            display.println("EEPROM!")
        end
        display.println("")
        display.println("Press button")
        display.println("to exit")
        display.unlock()
        display.refresh()
        return
    end
    
    display.println("Clear EEPROM")
    display.println("")
    display.println("WARNING:")
    display.println("This will delete")
    display.println("all saved settings")
    display.println("")
    
    local textHeight = display.textHeight()
    local textScale = display.textScale()
    local displayH = display.height()
    local buttonHeight = textHeight + 8 * textScale
    local buttonY = displayH - buttonHeight * 2 - 4
    
    gui.drawButton(0, buttonY, "Yes, Clear EEPROM", selected == 1, input.buttonRaw())
    local confirmBottomY = gui.getButtonBottomY(buttonY, "Yes, Clear EEPROM", selected == 1, 0)
    gui.drawButton(0, confirmBottomY + 2, "Cancel", selected == 2, input.buttonRaw())
    
    display.unlock()
    display.refresh()
end

function loop()
    if confirmed then
        if input.button() then
            app.exit()
        end
        app.delay(10)
        return
    end
    
    local needsRedraw = false
    
    if input.up() then
        if selected > 1 then
            selected = selected - 1
            needsRedraw = true
        end
    end
    
    if input.down() then
        if selected < MAX_ITEMS then
            selected = selected + 1
            needsRedraw = true
        end
    end
    
    if input.button() then
        if selected == 1 then
            confirmed = true
            draw_ui()
        else
            app.exit()
        end
        return
    end
    
    if needsRedraw then
        draw_ui()
    end
    
    app.delay(10)
end

