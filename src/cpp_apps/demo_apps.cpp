

#include "cpp_app.h"





CPP_APP(reaction) {
    randomSeed(analogRead(0));
    
    int bestTime = 9999;
    int lastTime = 0;
    int state = 0;  
    unsigned long stateStart = 0;
    unsigned long goTime = 0;
    
    auto render = [&]() {
        CppApp::clear();
        CppApp::println("Reaction Time");
        CppApp::println("");
        
        char buf[40];
        
        switch (state) {
            case 0:
                CppApp::println("Press button");
                CppApp::println("to start...");
                break;
            case 1:
                CppApp::println("Wait for it...");
                break;
            case 2:
                CppApp::println(">>> GO! <<<");
                CppApp::println("Press NOW!");
                break;
            case 3:
                snprintf(buf, sizeof(buf), "Time: %dms", lastTime);
                CppApp::println(buf);
                snprintf(buf, sizeof(buf), "Best: %dms", bestTime);
                CppApp::println(buf);
                CppApp::println("");
                CppApp::println("Button: again");
                break;
            case 4:
                CppApp::println("Too early!");
                CppApp::println("");
                CppApp::println("Button: retry");
                break;
        }
        
        CppApp::refresh();
    };
    
    render();
    
    while (!CppApp::shouldExit()) {
        bool btnPressed = CppApp::buttonRaw(); 
        bool btnReleased = CppApp::button();
        bool leftPressed = CppApp::left();
        
        
        if (leftPressed) {
            CppApp::exit();
            break;
        }
        
        switch (state) {
            case 0:  
                if (btnPressed) {
                    state = 1;
                    stateStart = millis();
                    render();
                }
                break;
                
            case 1:  
                if (btnPressed) {
                    state = 4;  
                    render();
                } else if (millis() - stateStart > (unsigned long)(1000 + random(3000))) {
                    state = 2;
                    goTime = millis();
                    render();
                }
                break;
                
            case 2:  
                if (btnPressed) {
                    lastTime = millis() - goTime;
                    if (lastTime < bestTime) {
                        bestTime = lastTime;
                    }
                    state = 3;
                    render();
                }
                break;
                
            case 3:  
            case 4:  
                if (btnReleased) {
                    state = 0;
                    render();
                }
                break;
        }
        
        CppApp::waitFrame(10);
    }
}

REGISTER_CPP_APP(reaction, "/Games/reaction");


void registerDemoApps() {
    registerCppApp("reaction", "/Games/reaction", cppapp_reaction);
}
