namespace EModLoader.Core;

public static class BuildFlavor
{
#if EML_COMPATIBILITY_TEST
    public static bool IsCompatibilityTest => true;
#else
    public static bool IsCompatibilityTest => false;
#endif
}
