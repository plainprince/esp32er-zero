



local TILE_SIZE = 8  
local HEADER_HEIGHT = 16  

local snake = {}
local dir = {x = 1, y = 0}
local food = {x = 0, y = 0}
local score = 0
local gameOver = false
local speed = 80  


local gridW = 0
local gridH = 0

function setup()
    math.randomseed(app.millis())
    
    
    gridW = math.floor(display.width() / TILE_SIZE)
    gridH = math.floor((display.height() - HEADER_HEIGHT) / TILE_SIZE)
    
    
    local startX = math.floor(gridW / 2)
    local startY = math.floor(gridH / 2)
    snake = {{x = startX, y = startY}}
    
    spawnFood()
    render()
end

function spawnFood()
    
    local valid = false
    while not valid do
        food.x = math.random(0, gridW - 1)
        food.y = math.random(0, gridH - 1)
        valid = true
        for i, seg in ipairs(snake) do
            if seg.x == food.x and seg.y == food.y then
                valid = false
                break
            end
        end
    end
end

function gridToPixel(gx, gy)
    return gx * TILE_SIZE, gy * TILE_SIZE + HEADER_HEIGHT
end

function render()
    display.clear()
    
    if gameOver then
        display.println("GAME OVER!")
        display.println("")
        display.print(0, 24, "Score: " .. tostring(score))
        display.println("")
        display.println("Button: exit")
    else
        display.print(0, 0, "Score: " .. tostring(score))
        
        
        for i, seg in ipairs(snake) do
            local px, py = gridToPixel(seg.x, seg.y)
            display.fillRect(px, py, TILE_SIZE - 1, TILE_SIZE - 1)
        end
        
        
        local fx, fy = gridToPixel(food.x, food.y)
        display.fillRect(fx + 2, fy + 2, TILE_SIZE - 5, TILE_SIZE - 5)
    end
    
    display.refresh()
end

function loop()
    
    if input.button() then
        app.exit()
        return
    end
    
    if gameOver then
        app.delay(100)
        return
    end
    
    
    if input.up() and dir.y == 0 then
        dir = {x = 0, y = -1}
    elseif input.down() and dir.y == 0 then
        dir = {x = 0, y = 1}
    elseif input.left() and dir.x == 0 then
        dir = {x = -1, y = 0}
    elseif input.right() and dir.x == 0 then
        dir = {x = 1, y = 0}
    end
    
    
    local head = snake[1]
    local newHead = {x = head.x + dir.x, y = head.y + dir.y}
    
    
    if newHead.x < 0 or newHead.x >= gridW or
       newHead.y < 0 or newHead.y >= gridH then
        gameOver = true
        render()
        return
    end
    
    
    for i, seg in ipairs(snake) do
        if newHead.x == seg.x and newHead.y == seg.y then
            gameOver = true
            render()
            return
        end
    end
    
    table.insert(snake, 1, newHead)
    
    
    if newHead.x == food.x and newHead.y == food.y then
        score = score + 10
        spawnFood()
        if speed > 40 then
            speed = speed - 3
        end
    else
        table.remove(snake)
    end
    
    render()
    app.delay(speed)
end

