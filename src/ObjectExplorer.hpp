#pragma once

#include <unordered_set>

#include "utility/Address.hpp"
#include "Mod.hpp"

class ObjectExplorer : public Mod {
public:
    ObjectExplorer();

    std::string_view getName() const override { return "ObjectExplorer"; };

    void onDrawUI() override;

private:
    void handleAddress(Address address, int32_t offset = -1, Address parent = nullptr);
    void handleGameObject(REGameObject* gameObject);
    void handleComponent(REComponent* component);
    void handleTransform(RETransform* transform);
    void handleType(REManagedObject* obj, REType* t);

    void attemptDisplayField(REManagedObject* obj, VariableDescriptor* desc);
    int32_t getFieldOffset(REManagedObject* obj, VariableDescriptor* desc);

    bool widgetWithContext(void* address, std::function<bool()> widget);
    void contextMenu(void* address);

    void makeTreeOffset(REManagedObject* object, uint32_t offset, std::string_view name);
    bool isManagedObject(Address address) const;

    std::string m_objectAddress{ "0" };
    std::chrono::system_clock::time_point m_nextRefresh;

    std::unordered_map<VariableDescriptor*, int32_t> m_offsetMap;
};