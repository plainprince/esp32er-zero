-- Random Password Generator
-- Generates secure random passwords

local chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*"
local length = 12
local password = ""
local needsRender = true

-- Simple PRNG using linear congruential generator
local seed = 0
local function random_seed(s)
    seed = s
end

local function random_int(min, max)
    -- Linear congruential generator: (a * seed + c) mod m
    -- Using values from Numerical Recipes
    seed = (seed * 1664525 + 1013904223) % 2147483647
    local range = max - min + 1
    return min + (seed % range)
end

local function random_password(len)
    -- Use table concat instead of string concatenation to avoid memory fragmentation
    local pwd = {}
    local chars_len = string.len(chars)
    for i = 1, len do
        local idx = random_int(1, chars_len)
        pwd[i] = string.sub(chars, idx, idx)
    end
    return table.concat(pwd)
end

function setup()
    random_seed(app.millis())
    password = random_password(length)
end

function loop()
    local btn = input.button()
    local left = input.left()
    local up = input.up()
    local down = input.down()
    
    if up then
        length = length + 1
        if length > 32 then length = 32 end
        password = random_password(length)
        needsRender = true
        app.delay(150)
    end
    
    if down then
        length = length - 1
        if length < 4 then length = 4 end
        password = random_password(length)
        needsRender = true
        app.delay(150)
    end
    
    if btn then
        password = random_password(length)
        needsRender = true
        app.delay(200) -- Debounce
    end
    
    if left then
        app.exit()
        return
    end
    
    if needsRender then
        display.clear()
        display.println("Password Gen")
        display.println("")
        display.println("Length: " .. length)
        display.println("")
        display.println("Password:")
        
        -- Wrap password at 21 chars
        local pwd_line1 = string.sub(password, 1, 21)
        display.println(pwd_line1)
        if string.len(password) > 21 then
            local pwd_line2 = string.sub(password, 22)
            display.println(pwd_line2)
        end
        
        display.println("")
        display.println("Btn:Gen U/D:Length")
        
        display.refresh()
        needsRender = false
    end
    
    app.delay(50)
end

