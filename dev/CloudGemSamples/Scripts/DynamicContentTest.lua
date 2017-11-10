
local DynamicContentTest = 
{
    Properties = 
    {
        UserRequestedPak = {default = "UserRequestedData.shared.pak"}
    },
}

function DynamicContentTest:UpdateGameProperties()
    if StaticDataRequestBus.Event == nil then
        Debug.Log("No StaticData Request Events found")
        return
    end   
    StaticDataRequestBus.Event.LoadRelativeFile(SystemEntityId,"StaticData/CSV/gameproperties.csv")
    StaticDataRequestBus.Event.LoadRelativeFile(SystemEntityId,"StaticData/CSV/userrequest.csv")
end

function DynamicContentTest:OnActivate()
	
    self.canvasEntityId = UiCanvasManagerBus.Broadcast.LoadCanvas("Levels/CloudGemTests/DynamicContentTest/UI/DynamicContentTest.uicanvas")

    self.requestList = {}
	
    -- Listen for action strings broadcast by the canvas
	self.buttonHandler = UiCanvasNotificationBus.Connect(self, self.canvasEntityId)
	
    local util = require("scripts.util")
    util.SetMouseCursorVisible(true)
   
      -- Listen for updates
    self.dynamicContentUpdateBus = DynamicContentUpdateBus.Connect(self, SystemEntityId)
    self.staticDataUpdateBus = StaticDataUpdateBus.Connect(self, SystemEntityId)
    self:UpdateGameProperties()
    self:UpdateUserDownloadable()
end

function DynamicContentTest:TypeReloaded(outputFile)
    Debug.Log("Static Data type reloaded:" .. outputFile)
    if outputFile == "gameproperties" or outputFile == "userrequest" then
        self:UpdateText()
    end
end

function DynamicContentTest:UpdateText()
    local displayEntity = UiCanvasBus.Event.FindElementByName(self.canvasEntityId, "PropertyText")
    local wasSuccess = false
    local returnValue = StaticDataRequestBus.Event.GetStrValue(SystemEntityId,"gameproperties","DynamicMessage", "Value", wasSuccess)
    Debug.Log("Got DynamicMessage value: " .. returnValue)   
    UiTextBus.Event.SetText(displayEntity, returnValue)

    local requestDisplayEntity = UiCanvasBus.Event.FindElementByName(self.canvasEntityId, "UserPropertyText")
    returnValue = StaticDataRequestBus.Event.GetStrValue(SystemEntityId,"userrequest","DynamicMessage", "Value", wasSuccess)
    Debug.Log("Got User Request DynamicMessage value: " .. returnValue)   
    UiTextBus.Event.SetText(requestDisplayEntity, returnValue)
end

function DynamicContentTest:RequestsCompleted()
    Debug.Log("Requests completed - refreshing")
    self:RefreshStatus()
end

function DynamicContentTest:RefreshStatus()
    self:UpdateGameProperties()
    self:UpdateUserDownloadable()
end

function DynamicContentTest:OnAction(entityId, actionName)

    if actionName == "RequestManifest" then
        Debug.Log("Requesting manifest..")
        if DynamicContentRequestBus.Event == nil then
            Debug.Log("No Content Request Events found")
            return
        end
        DynamicContentRequestBus.Event.RequestManifest(SystemEntityId,"DynamicContentTest.json")
        self:UpdateText()
    elseif actionName == "RequestPak" then
        DynamicContentRequestBus.Event.RequestFileStatus(SystemEntityId,self.Properties.UserRequestedPak, "")
    elseif actionName == "ClearContent" then
        DynamicContentRequestBus.Event.ClearAllContent(SystemEntityId)
        self:RefreshStatus()
    elseif actionName == "DeletePak" then
        DynamicContentRequestBus.Event.DeletePak(SystemEntityId,self.Properties.UserRequestedPak)
        self:RefreshStatus()
    end
end


function DynamicContentTest:OnDeactivate()
    DynamicContentRequestBus.Event.ClearAllContent(SystemEntityId)
    self.buttonHandler:Disconnect()
    self.dynamicContentUpdateBus:Disconnect()
    self.staticDataUpdateBus:Disconnect()
end

function DynamicContentTest:UpdateUserDownloadable()
    self.requestList = DynamicContentRequestBus.Event.GetDownloadablePaks(SystemEntityId)
    local numRequests = #self.requestList
    Debug.Log("Paks available for Download: " .. numRequests)
    local displayEntity = UiCanvasBus.Event.FindElementByName(self.canvasEntityId, "RequestPakText")
    fileStatus = DynamicContentRequestBus.Event.GetPakStatusString(SystemEntityId, self.Properties.UserRequestedPak)
    Debug.Log("Status for ".. self.Properties.UserRequestedPak .. " is ".. fileStatus)
	
    if fileStatus == "WAITING_FOR_USER" then
        Debug.Log("Download: " .. self.Properties.UserRequestedPak)
        UiTextBus.Event.SetText(displayEntity, self.Properties.UserRequestedPak)	
    elseif fileStatus == "INITIALIZED" or fileStatus == "READY" or fileStatus == "MOUNTED" then
        UiTextBus.Event.SetText(displayEntity, "File Downloaded")	
    else
        UiTextBus.Event.SetText(displayEntity, "No Paks Available")	
    end
end

return DynamicContentTest