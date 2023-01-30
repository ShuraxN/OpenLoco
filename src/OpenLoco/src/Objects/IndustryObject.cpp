#include "IndustryObject.h"
#include "CargoObject.h"
#include "Drawing/SoftwareDrawingEngine.h"
#include "Graphics/Colour.h"
#include "Graphics/Gfx.h"
#include "Localisation/StringIds.h"
#include "ObjectImageTable.h"
#include "ObjectManager.h"
#include "ObjectStringTable.h"
#include <OpenLoco/Interop/Interop.hpp>
#include <OpenLoco/Utility/Numeric.hpp>
#include <algorithm>

using namespace OpenLoco::Interop;

namespace OpenLoco
{
    bool IndustryObject::requiresCargo() const
    {
        auto requiredCargoState = false;
        for (const auto& requiredCargo : requiredCargoType)
        {
            if (requiredCargo != 0xff)
            {
                requiredCargoState = true;
                break;
            }
        }
        return requiredCargoState;
    }

    bool IndustryObject::producesCargo() const
    {
        auto produceCargoState = false;
        for (const auto& producedCargo : producedCargoType)
        {
            if (producedCargo != 0xff)
            {
                produceCargoState = true;
                break;
            }
        }
        return produceCargoState;
    }

    char* IndustryObject::getProducedCargoString(const char* buffer) const
    {
        char* ptr = (char*)buffer;
        auto producedCargoCount = 0;

        for (const auto& producedCargo : producedCargoType)
        {
            if (producedCargo != 0xFF)
            {
                producedCargoCount++;

                if (producedCargoCount > 1)
                    ptr = StringManager::formatString(ptr, StringIds::cargo_and);

                auto cargoObj = ObjectManager::get<CargoObject>(producedCargo);
                ptr = StringManager::formatString(ptr, cargoObj->name);
            }
        }
        return ptr;
    }

    char* IndustryObject::getRequiredCargoString(const char* buffer) const
    {
        char* ptr = (char*)buffer;
        auto requiredCargoCount = 0;

        for (const auto& requiredCargo : requiredCargoType)
        {
            if (requiredCargo != 0xFF)
            {
                requiredCargoCount++;

                if (requiredCargoCount > 1)
                {
                    if ((flags & IndustryObjectFlags::requiresAllCargo) != 0)
                        ptr = StringManager::formatString(ptr, StringIds::cargo_and);
                    else
                        ptr = StringManager::formatString(ptr, StringIds::cargo_or);
                }

                auto cargoObj = ObjectManager::get<CargoObject>(requiredCargo);
                ptr = StringManager::formatString(ptr, cargoObj->name);
            }
        }
        return ptr;
    }

    // 0x0045932D
    void IndustryObject::drawPreviewImage(Gfx::RenderTarget& rt, const int16_t x, const int16_t y) const
    {
        drawIndustry(&rt, x, y + 40);
    }

    // 0x00458C7F
    void IndustryObject::drawIndustry(Gfx::RenderTarget* clipped, int16_t x, int16_t y) const
    {
        auto firstColour = Utility::bitScanReverse(var_C2);
        Colour c = firstColour != -1 ? static_cast<Colour>(firstColour)
                                     : Colour::black;
        ImageId baseImage(var_12, c);
        Ui::Point pos{ x, y };
        auto& drawingCtx = Gfx::getDrawingEngine().getDrawingContext();
        for (const auto part : getBuildingParts(0))
        {
            auto image = baseImage.withIndexOffset(part * 4 + 1);
            drawingCtx.drawImage(*clipped, pos, image);
            pos.y -= buildingPartHeight[part];
        }
    }

    // 0x0045926F
    bool IndustryObject::validate() const
    {
        if (var_1E == 0)
        {
            return false;
        }
        if (var_1F == 0 || var_1F > 31)
        {
            return false;
        }

        if (var_BD < var_BC)
        {
            return false;
        }

        if (totalOfTypeInScenario == 0 || totalOfTypeInScenario > 32)
        {
            return false;
        }

        // 230/256 = ~90%
        if (-clearCostFactor > costFactor * 230 / 256)
        {
            return false;
        }

        if (var_E8 > 8)
        {
            return false;
        }
        switch (var_E9)
        {
            case 1:
            case 2:
            case 4:
                break;
            default:
                return false;
        }

        if (var_EA != 0xFF && var_EA > 7)
        {
            return false;
        }

        if (var_EC > 8)
        {
            return false;
        }

        if (var_D6 > 100)
        {
            return false;
        }
        return var_DA <= 100;
    }

    // 0x00458CD9
    void IndustryObject::load(const LoadedObjectHandle& handle, stdx::span<const std::byte> data, ObjectManager::DependentObjects* dependencies)
    {
        auto remainingData = data.subspan(sizeof(IndustryObject));

        {
            auto loadString = [&remainingData, &handle](string_id& dst, uint8_t num) {
                auto strRes = ObjectManager::loadStringTable(remainingData, handle, num);
                dst = strRes.str;
                remainingData = remainingData.subspan(strRes.tableLength);
            };
            string_id notUsed{};

            loadString(name, 0);
            loadString(var_02, 1);
            loadString(notUsed, 2);
            loadString(nameClosingDown, 3);
            loadString(nameUpProduction, 4);
            loadString(nameDownProduction, 5);
            loadString(nameSingular, 6);
            loadString(namePlural, 7);
        }

        // LOAD BUILDING PARTS Start
        // Load Part Heights
        buildingPartHeight = reinterpret_cast<const uint8_t*>(remainingData.data());
        remainingData = remainingData.subspan(var_1E * sizeof(uint8_t));

        // Load Part Animations
        buildingPartAnimations = reinterpret_cast<const BuildingPartAnimation*>(remainingData.data());
        remainingData = remainingData.subspan(var_1E * sizeof(BuildingPartAnimation));

        // Load Animations
        for (auto& animSeq : animationSequences)
        {
            animSeq = reinterpret_cast<const uint8_t*>(remainingData.data());
            // animationSequences comprises of a size then data. Size will always be a power of 2
            remainingData = remainingData.subspan(*animSeq * sizeof(uint8_t) + 1);
        }

        // Load Unk Animation Related Structure
        var_38 = reinterpret_cast<const IndustryObjectUnk38*>(remainingData.data());
        while (*remainingData.data() != static_cast<std::byte>(0xFF))
        {
            remainingData = remainingData.subspan(sizeof(IndustryObjectUnk38));
        }
        remainingData = remainingData.subspan(1);

        // Load Parts
        for (auto i = 0; i < var_1F; ++i)
        {
            auto& part = buildingParts[i];
            part = reinterpret_cast<const uint8_t*>(remainingData.data());
            while (*remainingData.data() != static_cast<std::byte>(0xFF))
            {
                remainingData = remainingData.subspan(1);
            }
            remainingData = remainingData.subspan(1);
        }
        // LOAD BUILDING PARTS End

        // Load Unk?
        var_BE = reinterpret_cast<const uint8_t*>(remainingData.data());
        remainingData = remainingData.subspan(var_BD * sizeof(uint8_t));

        // Load Produced Cargo
        for (auto& cargo : producedCargoType)
        {
            cargo = 0xFF;
            if (*remainingData.data() != static_cast<std::byte>(0xFF))
            {
                ObjectHeader cargoHeader = *reinterpret_cast<const ObjectHeader*>(remainingData.data());
                if (dependencies != nullptr)
                {
                    dependencies->required.push_back(cargoHeader);
                }
                auto res = ObjectManager::findObjectHandle(cargoHeader);
                if (res.has_value())
                {
                    cargo = res->id;
                }
            }
            remainingData = remainingData.subspan(sizeof(ObjectHeader));
        }

        // Load Required Cargo
        for (auto& cargo : requiredCargoType)
        {
            cargo = 0xFF;
            if (*remainingData.data() != static_cast<std::byte>(0xFF))
            {
                ObjectHeader cargoHeader = *reinterpret_cast<const ObjectHeader*>(remainingData.data());

                if (dependencies != nullptr)
                {
                    dependencies->required.push_back(cargoHeader);
                }
                auto res = ObjectManager::findObjectHandle(cargoHeader);
                if (res.has_value())
                {
                    cargo = res->id;
                }
            }
            remainingData = remainingData.subspan(sizeof(ObjectHeader));
        }

        // Load Wall Types
        for (auto& wallType : wallTypes)
        {
            wallType = 0xFF;
            if (*remainingData.data() != static_cast<std::byte>(0xFF))
            {
                ObjectHeader wallHeader = *reinterpret_cast<const ObjectHeader*>(remainingData.data());

                if (dependencies != nullptr)
                {
                    dependencies->required.push_back(wallHeader);
                }
                auto res = ObjectManager::findObjectHandle(wallHeader);
                if (res.has_value())
                {
                    wallType = res->id;
                }
            }
            remainingData = remainingData.subspan(sizeof(ObjectHeader));
        }

        // Load Unk1 Wall Types
        var_F1 = 0xFF;
        if (*remainingData.data() != static_cast<std::byte>(0xFF))
        {
            ObjectHeader unkHeader = *reinterpret_cast<const ObjectHeader*>(remainingData.data());
            if (dependencies != nullptr)
            {
                dependencies->required.push_back(unkHeader);
            }
            auto res = ObjectManager::findObjectHandle(unkHeader);
            if (res.has_value())
            {
                var_F1 = res->id;
            }
        }
        remainingData = remainingData.subspan(sizeof(ObjectHeader));

        // Load Unk2 Wall Types
        var_F2 = 0xFF;
        if (*remainingData.data() != static_cast<std::byte>(0xFF))
        {
            ObjectHeader unkHeader = *reinterpret_cast<const ObjectHeader*>(remainingData.data());
            if (dependencies != nullptr)
            {
                dependencies->required.push_back(unkHeader);
            }
            auto res = ObjectManager::findObjectHandle(unkHeader);
            if (res.has_value())
            {
                var_F2 = res->id;
            }
        }
        remainingData = remainingData.subspan(sizeof(ObjectHeader));

        // Load Image Offsets
        auto imgRes = ObjectManager::loadImageTable(remainingData);
        var_0E = imgRes.imageOffset;
        assert(remainingData.size() == imgRes.tableLength);
        var_12 = var_0E;
        if (flags & IndustryObjectFlags::hasShadows)
        {
            var_12 += var_1F * 4;
        }
        var_16 = var_1E * 4 + var_12;
        var_1A = var_E9 * 21;
    }

    // 0x0045919D
    void IndustryObject::unload()
    {
        name = 0;
        var_02 = 0;
        nameClosingDown = 0;
        nameUpProduction = 0;
        nameDownProduction = 0;
        nameSingular = 0;
        namePlural = 0;

        var_0E = 0;
        var_12 = 0;
        var_16 = 0;
        var_1A = 0;
        buildingPartHeight = nullptr;
        buildingPartAnimations = nullptr;
        std::fill(std::begin(animationSequences), std::end(animationSequences), nullptr);
        var_38 = nullptr;
        std::fill(std::begin(buildingParts), std::end(buildingParts), nullptr);
        var_BE = nullptr;
        std::fill(std::begin(producedCargoType), std::end(producedCargoType), 0);
        std::fill(std::begin(requiredCargoType), std::end(requiredCargoType), 0);
        std::fill(std::begin(wallTypes), std::end(wallTypes), 0);
        var_F1 = 0;
        var_F2 = 0;
    }

    stdx::span<const std::uint8_t> IndustryObject::getBuildingParts(const uint8_t buildingType) const
    {
        const auto* partsPointer = buildingParts[buildingType];
        auto* end = partsPointer;
        while (*end != 0xFF)
            end++;

        return stdx::span<const std::uint8_t>(partsPointer, end);
    }

    stdx::span<const std::uint8_t> IndustryObject::getAnimationSequence(const uint8_t unk) const
    {
        // animationSequences comprises of a size then data. Size will always be a power of 2
        const auto* sequencePointer = animationSequences[unk];
        const auto size = *sequencePointer++;
        return stdx::span<const std::uint8_t>(sequencePointer, size);
    }

    stdx::span<const IndustryObjectUnk38> OpenLoco::IndustryObject::getUnk38() const
    {
        const auto* unkPointer = var_38;
        auto* end = unkPointer;
        while (end->var_00 != 0xFF)
            end++;
        return stdx::span<const IndustryObjectUnk38>(unkPointer, end);
    }
}