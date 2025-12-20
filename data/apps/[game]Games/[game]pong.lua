


local paddleY = 28
local paddleH = 16
local ballX = 64
local ballY = 32
local ballDX = 4  
local ballDY = 2
local score = 0
local gameOver = false

function setup()
    render()
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
        display.print(50, 0, "Score: " .. tostring(score))
        
        
        display.fillRect(4, paddleY, 3, paddleH)
        
        
        display.fillRect(ballX, ballY, 4, 4)
        
        
        display.fillRect(display.width() - 2, 8, 2, display.height() - 8)
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
    
    
    local joyY = input.joystickY()
    paddleY = math.floor((joyY / 4095) * (display.height() - paddleH - 8)) + 8
    
    
    if input.up() and paddleY > 8 then
        paddleY = paddleY - 4
    end
    if input.down() and paddleY < display.height() - paddleH then
        paddleY = paddleY + 4
    end
    
    
    ballX = ballX + ballDX
    ballY = ballY + ballDY
    
    
    if ballY <= 8 or ballY >= display.height() - 4 then
        ballDY = -ballDY
    end
    
    
    if ballX >= display.width() - 6 then
        ballDX = -ballDX
        score = score + 1
    end
    
    
    if ballX <= 7 and ballY + 4 >= paddleY and ballY <= paddleY + paddleH then
        ballDX = -ballDX
        ballX = 8
        
        local hitPos = (ballY - paddleY) / paddleH
        ballDY = math.floor((hitPos - 0.5) * 4)
    end
    
    
    if ballX < 0 then
        gameOver = true
    end
    
    render()
    app.delay(15)  
end

