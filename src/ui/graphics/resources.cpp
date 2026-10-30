#include "resources.h"
#include <QResource>

void initializeUiResources()
{
    static const bool initialized = []
    {
        Q_INIT_RESOURCE(resources);
        return true;
    }();
    Q_UNUSED(initialized)
}
