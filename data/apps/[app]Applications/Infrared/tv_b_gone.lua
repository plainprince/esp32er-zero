-- TV-B-Gone (Lua version)
-- Sends IR POWER codes from database

local running = false
local current_index = 1
local sent_count = 0
local start_time = 0
local needsRender = true

local power_codes = {}

function setup()
    -- Load power codes from tv_codes.txt with error handling
    local success, err = pcall(function()
        local file = filesystem.open("/assets/tv_codes.txt", "r")
        if not file then
            error("Cannot open /assets/tv_codes.txt")
        end
        
        -- Read entire file as string
        local content = file:read()
        file:close()
        
        if not content or content == "" then
            error("File is empty")
        end
        
        -- Parse line by line
        -- Format: protocol:brand:button_name:address:command:nbits
        -- Example: NEC:LG:Power:0x20DF:0x10:32
        local lineCount = 0
        local parsedCount = 0
        for line in string.gmatch(content, "[^\r\n]+") do
            lineCount = lineCount + 1
            -- Skip comments and empty lines
            if not string.match(line, "^%s*#") and string.len(string.gsub(line, "%s+", "")) > 0 then
                -- Parse colon-separated format
                local protocol, brand, button_name, address_str, command_str, nbits_str = line:match("([^:]+):([^:]+):([^:]+):([^:]+):([^:]+):([^:]+)")
                if protocol and brand and address_str and command_str and nbits_str then
                    -- Convert hex strings to numbers (handle 0x prefix)
                    local function hexToNumber(hexStr)
                        -- Remove 0x prefix if present
                        hexStr = string.gsub(hexStr, "^0[xX]", "")
                        -- Convert hex to decimal
                        return tonumber(hexStr, 16)
                    end
                    
                    local address = hexToNumber(address_str)
                    local command = hexToNumber(command_str)
                    local nbits = tonumber(nbits_str)
                    
                    if address and command and nbits then
                        -- Filter for Power codes only (TV-B-Gone sends power codes)
                        -- Case-insensitive check for "power" in button name
                        local button_lower = string.lower(button_name)
                        if string.find(button_lower, "power") then
                            table.insert(power_codes, {
                                brand = brand,
                                protocol = protocol,
                                address = address,
                                command = command,
                                nbits = nbits
                            })
                            parsedCount = parsedCount + 1
                        end
                    end
                end
            end
        end
    end)
    
    if not success then
        display.clear()
        display.println("ERROR loading codes")
        display.println("")
        display.println(tostring(err))
        display.refresh()
        app.delay(3000)
        app.exit()
        return
    end
    
    needsRender = true
end

function loop()
    local btn = input.button()
    local left = input.left()
    
    if left then
        app.exit()
        return
    end
    
    if btn then
        if not running then
            running = true
            current_index = 1
            sent_count = 0
            start_time = app.millis()
            needsRender = true
        else
            running = false
            needsRender = true
        end
        app.delay(200) -- Debounce
    end
    
    -- Render UI only when status changes (on/off)
    if needsRender then
        display.clear()
        display.println("TV-B-Gone")
        display.println("")
        
        if running then
            display.println("Status: ON")
            display.println("")
            display.println("Sending codes...")
            display.println("")
            display.println("Press Btn: STOP")
        else
            if sent_count > 0 then
                display.println("Status: OFF")
                display.println("")
                display.println("Sent " .. sent_count .. " codes")
                display.println("")
                display.println("Press Btn: START")
            else
                display.println("Status: OFF")
                display.println("")
                display.println("Found " .. #power_codes .. " codes")
                display.println("")
                display.println("Press Btn: START")
            end
        end
        
        display.refresh()
        needsRender = false
    end
    
    -- Spam all packets without display updates for maximum performance
    if running then
        -- Send codes in tight loop with only 50ms delay between packets
        -- Loop continuously - restart from beginning when done
            if current_index > #power_codes then
                -- Restart from beginning for continuous operation
                current_index = 1
            end
            
        -- Bounds check to prevent errors
        if current_index >= 1 and current_index <= #power_codes then
            local code = power_codes[current_index]
            
            if code then
                -- Send the code with error handling
                local success, err = pcall(function()
            ir.send(code.protocol, code.address, code.command, code.nbits)
                end)
                
                if not success then
                    -- Log error but continue
                    -- Error sending code, skip it
                end
            
            current_index = current_index + 1
            sent_count = sent_count + 1
            else
                -- Invalid code, skip to next
                current_index = current_index + 1
            end
        else
            -- Reset if index is out of bounds
            current_index = 1
            end
            
            -- 50ms safety delay between codes
            app.delay(50)
    else
        -- Not running, small delay to prevent busy loop
        app.delay(50)
    end
end
