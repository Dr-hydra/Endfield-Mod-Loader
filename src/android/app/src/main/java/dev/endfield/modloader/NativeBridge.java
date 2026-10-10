package dev.endfield.modloader;
final class NativeBridge {
    static native boolean start(String index,String mode);
    static native boolean update(String index);
    static native void stop();
}
