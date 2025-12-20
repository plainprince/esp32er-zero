#ifndef LUA_SCRIPTS_H
#define LUA_SCRIPTS_H





const char* LUA_HELLO_WORLD = R"(
-- Hello World Lua App
-- Exits after 2 bounce cycles

local y = 0
local direction = 1
local bounces = 0

function setup()
    display.clear()
    display.println("Hello from Lua!")
    display.println("")
    display.println("This text is rendered")
    display.println("by a Lua script!")
    display.println("")
    display.println("Watch the bouncing bar...")
    display.refresh()
    app.delay(1000)
end

function loop()
    -- Check for exit via button
    if input.button() then
        app.exit()
        return
    end
    
    -- Simple animation - bouncing bar
    display.clear()
    display.println("Hello from Lua!")
    display.println("")
    
    -- Draw a moving bar
    display.fillRect(10, 20 + y, 50, 4)
    
    display.print(10, 50, "Bounces: " .. tostring(bounces))
    display.refresh()
    
    -- Update position
    y = y + direction
    if y > 20 then
        direction = -1
    elseif y < 0 then
        direction = 1
        bounces = bounces + 1
        -- Exit after 2 complete bounces
        if bounces >= 2 then
            display.clear()
            display.println("Done!")
            display.println("")
            display.println("Exiting...")
            display.refresh()
            app.delay(500)
            app.exit()
            return
        end
    end
    
    app.delay(50)
end
)";





const char* LUA_COUNTER = R"(
-- Counter App
-- Up/Down to change value
-- Button to exit

local count = 0

function setup()
    -- Initial render
    render()
end

function render()
    display.clear()
    display.println("Counter App")
    display.println("")
    display.print(10, 24, "Count: " .. tostring(count))
    display.println("")
    display.println("")
    display.println("Up/Down: change")
    display.println("Button: exit")
    display.refresh()
end

function loop()
    local changed = false
    
    if input.button() then
        app.exit()
        return
    end
    
    if input.up() then
        count = count + 1
        changed = true
    end
    
    if input.down() then
        count = count - 1
        changed = true
    end
    
    if changed then
        render()
    end
    
    app.delay(50)
end
)";





const char* LUA_DRAW_TEST = R"(
-- Draw Test
-- Shows graphics capabilities

function setup()
    local w = display.width()
    local h = display.height()
    
    display.clear()
    
    -- Title
    display.println("Graphics Test")
    
    -- Draw some rectangles
    display.drawRect(5, 15, 30, 20)
    display.fillRect(40, 15, 30, 20)
    display.drawRect(75, 15, 30, 20)
    
    -- Bottom text
    display.print(5, h - 10, "Press button")
    
    display.refresh()
end

function loop()
    if input.button() then
        app.exit()
    end
    app.delay(50)
end
)";

#endif 

