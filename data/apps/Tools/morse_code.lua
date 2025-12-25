-- Morse Code Transmitter
-- Blinks status LED in morse code

local morse = {
    A=".-", B="-...", C="-.-.", D="-..", E=".", F="..-.",
    G="--.", H="....", I="..", J=".---", K="-.-", L=".-..",
    M="--", N="-.", O="---", P=".--.", Q="--.-", R=".-.",
    S="...", T="-", U="..-", V="...-", W=".--", X="-..-",
    Y="-.--", Z="--..",
    ["0"]="-----", ["1"]=".----", ["2"]="..---", ["3"]="...--",
    ["4"]="....-", ["5"]=".....", ["6"]="-....", ["7"]="--...",
    ["8"]="---..", ["9"]="----.",
    [" "]="/"
}

local message = "SOS"
local sending = false
local done = false
local needsRender = true
local timing = 200  -- Base timing in ms (long = timing, short = timing/2, wait = timing/4)

function setup()
    -- Nothing
end

function loop()
    local btn = input.button()
    local left = input.left()
    local right = input.right()
    local up = input.up()
    local down = input.down()
    
    if left then
        app.exit()
        return
    end
    
    -- Change timing with up/down
    if up and not sending then
        timing = timing + 20
        if timing > 1000 then timing = 1000 end
        needsRender = true
        app.delay(100)
    end
    
    if down and not sending then
        timing = timing - 20
        if timing < 40 then timing = 40 end
        needsRender = true
        app.delay(100)
    end
    
    -- Open keyboard to edit message on right press
    if right and not sending then
        local newMessage = gui.keyboard("Edit Message", message)
        if newMessage then
            message = newMessage
        end
        needsRender = true
        app.delay(200)
    end
    
    if btn and not sending then
        sending = true
        done = false
        needsRender = true
        
        -- Update display
        display.clear()
        display.println("Morse Code")
        display.println("")
        display.println("Sending...")
        display.println("")
        display.println("Msg: " .. message)
        display.refresh()
        
        -- Send morse code via status LED
        local dit = timing / 2
        local dah = timing
        local symbol_space = timing / 4
        local letter_space = timing
        local word_space = timing * 2
        
        for i = 1, string.len(message) do
            local char = string.sub(message, i, i)
            local upperChar = string.upper(char)
            local code = morse[upperChar]
            if code then
                for j = 1, string.len(code) do
                    local symbol = string.sub(code, j, j)
                    if symbol == "." then
                        statusled.on()
                        app.delay(dit)
                        statusled.off()
                    elseif symbol == "-" then
                        statusled.on()
                        app.delay(dah)
                        statusled.off()
                    elseif symbol == "/" then
                        app.delay(word_space)
                    end
                    app.delay(symbol_space)
                end
                app.delay(letter_space)
            end
        end
        
        statusled.off()  -- Ensure LED is off
        sending = false
        done = true
        needsRender = true
        app.delay(200)
    end
    
    if needsRender then
        display.clear()
        display.println("Morse Code")
        display.println("")
        display.println("Msg: " .. message)
        display.println("Timing: " .. timing .. " ms")
        display.println("Long: " .. timing)
        display.println("Short: " .. (timing / 2))
        display.println("")
        
        if sending then
            display.println("Sending...")
        elseif done then
            display.println("Sent! Btn:Again")
            done = false  -- Reset for next render
        else
            display.println("Btn:Send  L:Exit")
            display.println("U/D:Time  R:Edit")
        end
        
        display.refresh()
        needsRender = false
    end
    
    app.delay(50)
end
