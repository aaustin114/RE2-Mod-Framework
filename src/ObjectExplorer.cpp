#include <sstream>

#include <windows.h>

#include "utility/String.hpp"
#include "utility/Scan.hpp"

#include "REFramework.hpp"
#include "ObjectExplorer.hpp"

ObjectExplorer::ObjectExplorer()
{
    m_objectAddress.reserve(256);
}

void ObjectExplorer::onDrawUI() {
    ImGui::SetNextTreeNodeOpen(false, ImGuiCond_::ImGuiCond_Once);

    if (!ImGui::CollapsingHeader(getName().data())) {
        return;
    }

    auto curtime = std::chrono::system_clock::now();

    // List of globals to choose from
    if (ImGui::CollapsingHeader("Singletons")) {
        if (curtime > m_nextRefresh) {
            g_framework->getGlobals()->safeRefresh();
            m_nextRefresh = curtime + std::chrono::seconds(1);
        }

        // make a copy, we want to sort by name
        auto singletons = g_framework->getGlobals()->getObjects();

        // first loop, sort
        std::sort(singletons.begin(), singletons.end(), [](REManagedObject** a, REManagedObject** b) {
            auto aType = utility::REManagedObject::safeGetType(*a);
            auto bType = utility::REManagedObject::safeGetType(*b);

            if (aType == nullptr || aType->name == nullptr) {
                return true;
            }

            if (bType == nullptr || bType->name == nullptr) {
                return false;
            }

            return std::string_view{ aType->name } < std::string_view{ bType->name };
        });

        // Display the nodes
        for (auto obj : singletons) {
            auto t = utility::REManagedObject::safeGetType(*obj);

            if (t == nullptr || t->name == nullptr) {
                continue;
            }

            ImGui::SetNextTreeNodeOpen(false, ImGuiCond_::ImGuiCond_Once);

            if (ImGui::TreeNode(t->name)) {
                handleAddress(*obj);
                ImGui::TreePop();
            }

            contextMenu(*obj);
        }
    }

    ImGui::InputText("REObject Address", m_objectAddress.data(), 16, ImGuiInputTextFlags_::ImGuiInputTextFlags_CharsHexadecimal);

    if (m_objectAddress[0] != 0) {
        handleAddress(std::stoull(m_objectAddress, nullptr, 16));
    }
}

void ObjectExplorer::handleAddress(Address address, int32_t offset, Address parent) {
    if (!isManagedObject(address)) {
        return;
    }

    auto object = address.as<REManagedObject*>();
    
    if (parent == nullptr) {
        parent = address;
    }

    bool madeNode = false;
    auto isGameObject = utility::REManagedObject::isA(object, "via.GameObject");

    if (offset != -1) {
        ImGui::SetNextTreeNodeOpen(false, ImGuiCond_::ImGuiCond_Once);

        if (isGameObject) {
            madeNode = ImGui::TreeNode(parent.get(offset), "0x%X: %s", offset, utility::REString::getString(address.as<REGameObject*>()->name).c_str());
        }
        else {
            madeNode = ImGui::TreeNode(parent.get(offset), "0x%X: %s", offset, object->info->classInfo->type->name);
        }

        contextMenu(object);
    }

    if (madeNode || offset == -1) {
        if (isGameObject) {
            handleGameObject(address.as<REGameObject*>());
        }

        if (utility::REManagedObject::isA(object, "via.Component")) {
            handleComponent(address.as<REComponent*>());
        }

        handleType(object, utility::REManagedObject::getType(object));

        if (ImGui::TreeNode(object, "AutoGenerated Types")) {
            auto typeInfo = object->info->classInfo->type;
            auto size = utility::REManagedObject::getSize(object);

            for (auto i = (uint32_t)sizeof(void*); i < size; i += sizeof(void*)) {
                auto ptr = Address(object).get(i).to<REManagedObject*>();

                handleAddress(ptr, i, object);
            }

            ImGui::TreePop();
        }
    }

    if (madeNode && offset != -1) {
        ImGui::TreePop();
    }
}

void ObjectExplorer::handleGameObject(REGameObject* gameObject) {
    ImGui::Text("Name: %s", utility::REString::getString(gameObject->name).c_str());
    makeTreeOffset(gameObject, offsetof(REGameObject, transform), "Transform");
    makeTreeOffset(gameObject, offsetof(REGameObject, folder), "Folder");
}

void ObjectExplorer::handleComponent(REComponent* component) {
    makeTreeOffset(component, offsetof(REComponent, ownerGameObject), "Owner");
    makeTreeOffset(component, offsetof(REComponent, childComponent), "ChildComponent");
    makeTreeOffset(component, offsetof(REComponent, prevComponent), "PrevComponent");
    makeTreeOffset(component, offsetof(REComponent, nextComponent), "NextComponent");
}

void ObjectExplorer::handleTransform(RETransform* transform) {

}

void ObjectExplorer::handleType(REManagedObject* obj, REType* t) {
    if (obj == nullptr || t == nullptr) {
        return;
    }

    auto count = 0;

    for (auto typeInfo = t; typeInfo != nullptr; typeInfo = typeInfo->super) {
        auto name = typeInfo->name;

        if (name == nullptr) {
            continue;
        }

        if (!ImGui::TreeNode(name)) {
            break;
        }

        // Topmost type
        if (typeInfo == t) {
            ImGui::Text("Size: 0x%X", utility::REManagedObject::getSize(obj));
        }
        // Super types
        else {
            ImGui::Text("Size: 0x%X", typeInfo->size);
        }

        ++count;

        if (typeInfo->fields == nullptr || typeInfo->fields->variables == nullptr || typeInfo->fields->variables->data == nullptr) {
            continue;
        }

        auto descriptors = typeInfo->fields->variables->data->descriptors;

        if (ImGui::TreeNode(typeInfo->fields, "Fields: %i", typeInfo->fields->variables->num)) {
            for (auto i = descriptors; i != descriptors + typeInfo->fields->variables->num; ++i) {
                auto variable = *i;

                if (variable == nullptr) {
                    continue;
                }

                auto madeNode = widgetWithContext(variable->function, [&]() { return ImGui::TreeNode(variable, "%s", variable->typeName); });
                auto treeHovered = ImGui::IsItemHovered();

                // Draw the variable name with a color
                ImGui::SameLine();
                ImGui::TextColored(ImVec4{ 100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 255 / 255.0f }, "%s", variable->name);

                // Display the field offset
                auto offset = getFieldOffset(obj, variable);

                if (offset != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4{ 1.0f, 0.0f, 0.0f, 1.0f }, "0x%X", offset);
                }
                /*else if (treeHovered || ImGui::IsItemHovered()) {
                    m_offsetMap.erase(variable);
                }*/

                // Info about the field
                if (madeNode) {
                    attemptDisplayField(obj, variable);

                    if (ImGui::TreeNode(variable, "Additional Information")) {
                        ImGui::Text("Address: 0x%p", variable);
                        ImGui::Text("Function: 0x%p", variable->function);

                        if (variable->typeName != nullptr) {
                            ImGui::Text("Type: %s", variable->typeName);
                        }

                        ImGui::Text("Flags & 1F: 0x%X", variable->flags & 0x1F);
                        ImGui::Text("VarType: %i", variable->variableType);

                        if (variable->staticVariableData != nullptr) {
                            ImGui::Text("GlobalIndex: %i", variable->staticVariableData->variableIndex);
                        }
                    }

                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }
    }

    for (auto i = 0; i < count; ++i) {
        ImGui::TreePop();
    }
}

void ObjectExplorer::attemptDisplayField(REManagedObject* obj, VariableDescriptor* desc) {
    if (desc->function == nullptr) {
        return;
    }

    auto makeTreeAddr = [this](void* addr) {
        if (widgetWithContext(addr, [&]() { return ImGui::TreeNode(addr, "Variable: 0x%p", addr); })) {
            if (isManagedObject(addr)) {
                handleAddress(addr);
            }

            ImGui::TreePop();
        }
    };

    auto ret = utility::hash(std::string{ desc->typeName });
    auto getValueFunc = (void* (*)(VariableDescriptor*, REManagedObject*, void*))desc->function;

    char data[0x100]{ 0 };

    // 0x10 == pointer, i think?
    if (((desc->flags & 0x1F) - 1 != 0x10) || desc->staticVariableData == nullptr) {
        getValueFunc(desc, obj, &data);

        // yay for compile time string hashing
        switch (ret) {
        case "s32"_fnv:
            ImGui::Text("%i", *(int16_t*)&data);
            break;
        case "u64"_fnv:
            ImGui::Text("%llu", *(int64_t*)&data);
            break;
        case "u32"_fnv:
            ImGui::Text("%i", *(int32_t*)&data);
            break;
        case "System.Nullable`1<System.Single>"_fnv:
        case "f32"_fnv:
            ImGui::Text("%f", *(float*)&data);
            break;
        case "System.Nullable`1<System.Boolean>"_fnv:
        case "bool"_fnv:
            if (*(bool*)&data) {
                ImGui::Text("true");
            }
            else {
                ImGui::Text("false");
            }
            break;
        case "c16"_fnv:
            if (*(wchar_t**)&data == nullptr) {
                break;
            }

            ImGui::Text("%s", utility::narrow(*(wchar_t**)&data).c_str());
            break;
        case "c8"_fnv:
            if (*(char**)&data == nullptr) {
                break;
            }

            ImGui::Text("%s", *(char**)&data);
            break;
        case "System.Nullable`1<via.vec2>"_fnv:
        case "via.vec2"_fnv:
        {
            auto& vec = *(Vector2f*)&data;
            ImGui::Text("%f %f", vec.x, vec.y);
            break;
        }
        case "System.Nullable`1<via.vec3>"_fnv:
        case "via.vec3"_fnv:
        {
            auto& vec = *(Vector3f*)&data;
            ImGui::Text("%f %f %f", vec.x, vec.y, vec.z);
            break;
        }
        case "via.Quaternion"_fnv:
        {
            auto& quat = *(glm::quat*)&data;
            ImGui::Text("%f %f %f %f", quat.x, quat.y, quat.z, quat.w);
            break;
        }
        case "via.string"_fnv:
            ImGui::Text("%s", utility::REString::getString(*(REString*)&data).c_str());
            break;
        default:
            if ((desc->flags & 0x1F) == 1) {
                ImGui::Text("%i", *(int32_t*)&data);
            }
            else {
                makeTreeAddr(*(void**)&data);
            }

            break;
        }
    }
    // Pointer... usually
    else {
        getValueFunc(desc, obj, &data);
        makeTreeAddr(*(void**)&data);
    }
}

int32_t ObjectExplorer::getFieldOffset(REManagedObject* obj, VariableDescriptor* desc) {
    if (desc->typeName == nullptr || desc->function == nullptr || m_offsetMap.find(desc) != m_offsetMap.end()) {
        return m_offsetMap[desc];
    }

    auto ret = utility::hash(std::string{ desc->typeName });

    // These usually modify the object state, not what we want.
    if (ret == "undefined"_fnv) {
        return m_offsetMap[desc];
    }

    // Set up our "translator" to throw on any exception,
    // Particularly access violations.
    // Kind of gross but it's necessary for some fields,
    // because the field function may access the thing we modified, which may actually be a pointer,
    // and we need to handle it.
    _set_se_translator([](uint32_t code, EXCEPTION_POINTERS* exc) {
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        {
            spdlog::info("ObjectExplorer: Attempting to handle access violation.");

            // idk. best name i could come up with.
            static void** dotNETContext = nullptr;
            static uint8_t* (*getThreadNETContext)(void*, int) = nullptr;

            if (dotNETContext == nullptr) {
                auto ref = utility::scan(g_framework->getModule().as<HMODULE>(), "48 8B 0D ? ? ? ? BA FF FF FF FF E8 ? ? ? ? 48 89 C3");

                if (!ref) {
                    spdlog::info("Unable to find ref. We are going to crash.");
                    break;
                }

                dotNETContext = (void**)utility::calculateAbsolute(*ref + 3);
                getThreadNETContext = (decltype(getThreadNETContext))utility::calculateAbsolute(*ref + 13);

                spdlog::info("g_dotNETContext: {:x}", (uintptr_t)dotNETContext);
                spdlog::info("getThreadNETContext: {:x}", (uintptr_t)getThreadNETContext);
            }

            auto someObject = Address{ getThreadNETContext(*dotNETContext, -1) };

            // This counter needs to be dealt with, it will end up causing a crash later on.
            // We also need to "destruct" whatever object this is.
            if (someObject != nullptr) {
                auto& referenceCount = *someObject.get(0x78).as<int32_t*>();

                spdlog::error("{}", referenceCount);
                if (referenceCount > 0) {
                    --referenceCount;

                    static void* (*func1)(void*) = nullptr;
                    static void* (*func2)(void*) = nullptr;
                    static void* (*func3)(void*) = nullptr;

                    // Get our function pointers
                    if (func1 == nullptr) {
                        spdlog::info("Locating funcs");

                        auto ref = utility::scan(g_framework->getModule().as<HMODULE>(), "48 83 78 18 00 74 ? 48 89 D9 E8 ? ? ? ? 48 89 D9 E8 ? ? ? ?");

                        if (!ref) {
                            spdlog::error("We're going to crash");
                            break;
                        }

                        func1 = Address{ utility::calculateAbsolute(*ref + 11) }.as<decltype(func1)>();
                        func2 = Address{ utility::calculateAbsolute(*ref + 19) }.as<decltype(func2)>();
                        func3 = Address{ utility::calculateAbsolute(*ref + 27) }.as<decltype(func3)>();

                        spdlog::info("F1 {:x}", (uintptr_t)func1);
                        spdlog::info("F2 {:x}", (uintptr_t)func2);
                        spdlog::info("F3 {:x}", (uintptr_t)func3);
                    }

                    // Perform object cleanup that was missed because an exception occurred.
                    if (someObject.get(0x50).deref().get(0x18).deref() != nullptr) {
                        func1(someObject);                   
                    }

                    func2(someObject);
                    func3(someObject);
                }
                else {
                    spdlog::info("Reference count was 0.");
                }
            }
            else {
                spdlog::info("thread context was null. A crash may occur.");
            }
        }
        default:
            break;
        }

        throw std::exception(std::to_string(code).c_str());
    });

    struct BitTester {
        BitTester(uint8_t* oldValue)
            : ptr{ oldValue }
        {
            old = *oldValue;
        }

        ~BitTester() {
            *ptr = old;
        }

        bool isValueSame(const uint8_t* buf) const {
            return buf[0] == ptr[0];
        }

        uint8_t* ptr;
        uint8_t old;
    };

    const auto getValueFunc = (void* (*)(VariableDescriptor*, REManagedObject*, void*))desc->function;
    const auto classSize = utility::REManagedObject::getSize(obj);
    const auto size = 1;

    // Copy the object so we don't cause a crash by replacing
    // data that's being used by the game
    std::vector<uint8_t> objectCopy;
    objectCopy.reserve(classSize);
    memcpy(objectCopy.data(), obj, classSize);

    // Compare data
    for (int32_t i = sizeof(REManagedObject); i + size <= (int32_t)classSize; i += 1) {
        auto ptr = objectCopy.data() + i;
        bool same = true;

        BitTester tester{ ptr };

        // Compare data twice, first run no modifications,
        // second run, slightly modify the data to double check if it's what we want.
        for (int32_t k = 0; k < 2; ++k) {
            std::array<uint8_t, 0x100> data{ 0 };

            // Attempt to get the field value.
            try {
                getValueFunc(desc, (REManagedObject*)objectCopy.data(), data.data());
            }
            // Access violation occurred. Good thing we handle it.
            catch (const std::exception&) {
                same = false;
                break;
            }

            // Check if result is the same at our offset.
            if (!tester.isValueSame(data.data())) {
                same = false;
                break;
            }

            // Modify the data for our second run.
            *ptr ^= 1;
        }

        if (same) {
            m_offsetMap[desc] = i;
            break;
        }
    }

    return m_offsetMap[desc];
}

bool ObjectExplorer::widgetWithContext(void* address, std::function<bool()> widget) {
    auto ret = widget();
    contextMenu(address);

    return ret;
}

void ObjectExplorer::contextMenu(void* address) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::Selectable("Copy")) {
            std::stringstream ss;
            ss << std::hex << (uintptr_t)address;

            ImGui::SetClipboardText(ss.str().c_str());
        }

        // Log component hierarchy to disk
        if (isManagedObject(address) && utility::REManagedObject::isA((REManagedObject*)address, "via.Component") && ImGui::Selectable("Log Hierarchy")) {
            auto comp = (REComponent*)address;

            for (auto obj = comp; obj; obj = obj->childComponent) {
                auto t = utility::REManagedObject::safeGetType(obj);

                if (t != nullptr) {
                    if (obj->ownerGameObject == nullptr) {
                        spdlog::info("{:s} ({:x})", t->name, (uintptr_t)obj);
                    }
                    else {
                        auto owner = obj->ownerGameObject;
                        spdlog::info("[{:s}] {:s} ({:x})", utility::REString::getString(owner->name), t->name, (uintptr_t)obj);
                    }
                }

                if (obj->childComponent == comp) {
                    break;
                }
            }
        }

        ImGui::EndPopup();
    }
}

void ObjectExplorer::makeTreeOffset(REManagedObject* object, uint32_t offset, std::string_view name) {
    auto ptr = Address(object).get(offset).to<void*>();

    if (ptr == nullptr) {
        return;
    }

    auto madeNode = ImGui::TreeNode((uint8_t*)object + offset, "0x%X: %s", offset, name.data());

    contextMenu(ptr);

    if (madeNode) {
        handleAddress(ptr);
        ImGui::TreePop();
    }
}

bool ObjectExplorer::isManagedObject(Address address) const {
    return utility::REManagedObject::isManagedObject(address);
}
