menu = {}

function menu:new(instance)
    instance = instance or {}
    self.__index = self
    setmetatable(instance, self)
    return instance
end

function menu:Show()
    if not self.canvasName then
        Debug.Log("menu:Show: Missing canvasName")
        return
    end
    
    if self.canvasEntityId then
        Debug.Log("menu:Show: Canvas " .. self.canvasName .. " already showing: " .. tostring(self.canvasEntityId))
        return
    end

    local canvasPath = "UI/Canvases/PlayerAccount/" .. self.canvasName .. ".uicanvas"
    self.canvasEntityId = UiCanvasManagerBus.Broadcast.LoadCanvas(canvasPath)
    
    if not self.canvasEntityId then
        Debug.Log("Failed to load canvas " .. canvasPath)
        return
    end

    self.canvasHandler = UiCanvasNotificationBus.Connect(self, self.canvasEntityId)
    self:ConnectToCloudGemPlayerAccountNotificationHandler()
end

function menu:ConnectToCloudGemPlayerAccountNotificationHandler()
    if not self.cloudGemPlayerAccountNotificationHandler then
        self.cloudGemPlayerAccountNotificationHandler = CloudGemPlayerAccountNotificationBus.Connect(self, self.entityId)
    end
end

function menu:Hide()
    if self.canvasHandler then
        self.canvasHandler:Disconnect()
        self.canvasHandler = nil
    end
    if self.canvasEntityId then
        UiCanvasManagerBus.Broadcast.UnloadCanvas(self.canvasEntityId)
        self.canvasEntityId = nil
    end
    if self.cloudGemPlayerAccountNotificationHandler then
        self.cloudGemPlayerAccountNotificationHandler:Disconnect()
        self.cloudGemPlayerAccountNotificationHandler = nil
    end
end

function menu:OnAction(entityId, actionName)
    local menuName = string.match(actionName, "^Show([a-zA-Z]+)Click$")
    if menuName then
        self.menuManager:ShowMenu(menuName)
        return
    end
    
    Debug.Log("menu:OnAction: Unrecognized action '" .. actionName .. "'")
end

function menu:OnAfterConfigurationChange()
    CloudGemPlayerAccountRequestBus.Broadcast.GetCurrentUser()
end

function menu:OnGetCurrentUserComplete(result)
    if not result.wasSuccessful and self.loadMainMenuOnSignOut then
        Debug.Log("Not signed in, loading main menu.")
        self:RunOnMainThread(function()
            self.menuManager:ShowMenu("MainMenu")
        end)
    end
end

function menu:GetText(elementName)
    local elementId = UiCanvasBus.Event.FindElementByName(self.canvasEntityId, elementName)
    return UiTextBus.Event.GetText(elementId)
end

function menu:SetText(elementName, text)
    local elementId = UiCanvasBus.Event.FindElementByName(self.canvasEntityId, elementName)
    UiTextBus.Event.SetText(elementId, text)
end

function menu:RunOnMainThread(task)
    self.menuManager:RunOnMainThread(task)
end

return menu
