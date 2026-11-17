//
//  VoodooGPIOMeteorLakeP.cpp
//  VoodooGPIO
//
//  Meteor Lake-P (INTC1083) / Arrow Lake-H (INTC105E). Added 2026-11-17 (GU605MY).
//

#include "VoodooGPIOMeteorLakeP.hpp"

OSDefineMetaClassAndStructors(VoodooGPIOMeteorLakeP, VoodooGPIOIntel);

bool VoodooGPIOMeteorLakeP::start(IOService *provider) {
    this->pins = mtlp_pins;
    this->npins = ARRAY_SIZE(mtlp_pins);
    this->communities = mtlp_communities;
    this->ncommunities = ARRAY_SIZE(mtlp_communities);

    IOLog("%s::Loading GPIO Data for MeteorLake-P\n", getName());

    return VoodooGPIOIntel::start(provider);
}
