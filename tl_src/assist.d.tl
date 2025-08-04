
--[[
local record Assist
    new: function(): Assist
    send: function(Assist, query: string): string
    models_list: function(Assist): {string}
end
--]]

local record Assist

    enum LLMRole
        "system"
        "user"
        "assistant"
    end

    record LLMMessage
        role: LLMRole
        content: string
    end

    record LLMPayload
        model: string
        messages: {LLMMessage}
        stream: boolean
        max_tokens: integer
    end

    send2llm: function(LLMPayload, function(responce_chunk: string)): string
    models_list: function(): {string}
    embedding: function(modelname: string, text: string|{string}): {{number}}
end

return Assist
