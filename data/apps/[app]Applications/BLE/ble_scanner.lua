-- BLE Scanner
-- Scans for nearby Bluetooth devices

local devices = {}
local selectedIndex = 1
local scanning = false
local needsRender = true

function setup()
    -- Clean up any existing BLE objects before starting
    ble.cleanup()
    needsRender = true
end

function loop()
    local btn = input.button()
    local left = input.left()
    local up = input.up()
    local down = input.down()
    
    if left then
        -- Clean up BLE before exiting
        ble.cleanup()
        app.exit()
        return
    end
    
    -- Start scan on button press
    if btn and not scanning then
        scanning = true
        needsRender = true
        devices = {}
        
        -- Show scanning status
        display.clear()
        display.println("BLE Scanner")
        display.println("")
        display.println("Scanning...")
        display.println("")
        display.println("Please wait...")
        display.refresh()
        
        -- Scan for 5 seconds
        local success = ble.scan(5)
        
        if success then
            -- Get results
            local count = ble.getCount()
            for i = 1, count do
                local device = ble.getDevice(i)
                if device and device.name then
                    -- Copy device data into a new persistent table to avoid memory leaks
                    local devCopy = {
                        name = device.name,
                        address = device.address,
                        rssi = device.rssi
                    }
                    table.insert(devices, devCopy)
                end
            end
        else
            -- Scan failed
            display.clear()
            display.println("BLE Scanner")
            display.println("")
            display.println("Scan failed!")
            display.println("")
            display.println("Try again")
            display.refresh()
            app.delay(2000)
        end
        
        scanning = false
        selectedIndex = 1
        needsRender = true
    end
    
    -- Navigation
    if up and selectedIndex > 1 then
        selectedIndex = selectedIndex - 1
        needsRender = true
        app.delay(150)
    end
    
    if down and selectedIndex < #devices then
        selectedIndex = selectedIndex + 1
        needsRender = true
        app.delay(150)
    end
    
    -- Render only when needed
    if needsRender then
        display.clear()
        display.println("BLE Scanner")
        display.println("")
        
        if #devices == 0 then
            display.println("No devices found")
            display.println("")
            display.println("Btn: Scan")
        else
            display.println("Found: " .. #devices)
            display.println("")
            
            -- Show 3 devices at a time
            local maxVisible = 3
            local startIdx = selectedIndex - 1
            if startIdx < 1 then startIdx = 1 end
            local endIdx = startIdx + maxVisible - 1
            if endIdx > #devices then endIdx = #devices end
            
            for i = startIdx, endIdx do
                local dev = devices[i]
                if dev and dev.name then
                    local prefix = (i == selectedIndex) and "> " or "  "
                    
                    -- Truncate name if too long
                    local name = dev.name
                    if string.len(name) > 16 then
                        name = string.sub(name, 1, 13) .. "..."
                    end
                    
                    display.println(prefix .. name)
                end
            end
            
            display.println("")
            display.println("Btn:Scan U/D:Nav")
        end
        
        display.refresh()
        needsRender = false
    end
    
    app.delay(50)
end
