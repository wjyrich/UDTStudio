#include "texgenerator.h"

TexGenerator::TexGenerator()
{
}

TexGenerator::~TexGenerator()
{
}

bool TexGenerator::generate(DeviceConfiguration *deviceConfiguration, const QString &filePath)
{
    Q_UNUSED(deviceConfiguration);
    Q_UNUSED(filePath);
    return true;
}

bool TexGenerator::generate(DeviceDescription *deviceDescription, const QString &filePath)
{
    Q_UNUSED(deviceDescription);
    Q_UNUSED(filePath);
    return true;
}
