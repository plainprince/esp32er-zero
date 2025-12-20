


local inputApp

function setup()
    inputApp = gui.createApp("Input Demo", {
        {type="input", label="Input 1", value="", x=0, w=128},
        {type="input", label="Input 2", value="", x=0, w=128},
        {type="input", label="Input 3", value="", x=0, w=128},
        {type="button", label="Submit & Exit", callback=function()
            
            local val1 = inputApp.getInputValue(1)
            local val2 = inputApp.getInputValue(2)
            local val3 = inputApp.getInputValue(3)
            
            
            inputApp.exit()
            
            
            display.clear()
            display.println("Submitted Values:")
            display.println("")
            display.println("Input 1: " .. val1)
            display.println("Input 2: " .. val2)
            display.println("Input 3: " .. val3)
            display.println("")
            local isBusy = display.busy()
            display.println("Screen busy: " .. (isBusy and "YES" or "NO"))
            display.refresh()
            
            
            app.delay(2000)
            
            
            gui.showLoadingScreen()
            display.refresh()
            app.delay(2000)
            
            
            gui.hideLoadingScreen()
            display.refresh()
            app.delay(500)
            
            
            gui.showLoadingScreen()
            display.refresh()
            app.delay(1500)
            
            
            gui.hideLoadingScreen()
            display.refresh()
            app.delay(500)
            app.exit()  
        end}
    })
end

function loop()
    inputApp.update()  
    app.delay(10)
end

