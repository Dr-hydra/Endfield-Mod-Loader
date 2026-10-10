package dev.endfield.modloader;
import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.ParcelFileDescriptor;
import io.github.libxposed.api.XposedModule;
import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Supplier;

public final class XposedEntry extends XposedModule {
    private String processName;private final AtomicBoolean attached=new AtomicBoolean();private final AtomicBoolean loaded=new AtomicBoolean();
    private final AtomicBoolean loading=new AtomicBoolean();private final java.util.concurrent.atomic.AtomicInteger attempts=new java.util.concurrent.atomic.AtomicInteger();
    @Override public void onModuleLoaded(ModuleLoadedParam param){processName=param.getProcessName();}
    @Override public void onPackageReady(PackageReadyParam param){
        String packageName=param.getPackageName();if(!packageName.equals(processName)||!(packageName.equals("com.hypergryph.endfield")||packageName.equals("com.gryphline.endfield.gp"))||!attached.compareAndSet(false,true))return;
        try{hook(Application.class.getDeclaredMethod("attach",Context.class)).intercept(chain->{Object result=chain.proceed();Context context=(Context)chain.getArg(0);
            Application application=(Application)chain.getThisObject();
            new Thread(()->prepare(context,param.getClassLoader(),application.getClass()),"EML-Prepare").start();return result;});}
        catch(Throwable error){android.util.Log.e("EModLoader","Framework entry failed",error);}
    }
    // Runs only in the framework-authorized game process. Denied private API
    // access is reported and leaves EML disabled; no API-access workaround.
    @android.annotation.SuppressLint("BlockedPrivateApi")
    private void prepare(Context context,ClassLoader gameLoader,Class<?> gameApplication){
        try{
            Supplier<SharedPreferences> settings=()->getRemotePreferences("module_settings");
            String initial=settings.get().getString(ModuleStore.KEY,"");if(initial.isEmpty())return;
            RuntimeMaterializer.Source source=name->new ParcelFileDescriptor.AutoCloseInputStream(openRemoteFile(name));
            String index=RuntimeMaterializer.prepare(context,initial,source,text->android.util.Log.i("EModLoader",text));
            Class<?> unity=null;for(int i=0;i<120&&unity==null;i++){try{unity=Class.forName("com.unity3d.player.UnityPlayer",false,gameLoader);}catch(ClassNotFoundException absent){Thread.sleep(100);}}
            if(unity==null)throw new ClassNotFoundException("UnityPlayer");
            for(Method method:unity.getDeclaredMethods())if(method.getName().equals("nativeRender")&&method.getReturnType()==boolean.class){
                hook(method).intercept(chain->{Object result=chain.proceed();if(Boolean.TRUE.equals(result)&&!loaded.get()&&attempts.get()<3&&loading.compareAndSet(false,true)){
                    attempts.incrementAndGet();
                    try{Context module=context.createPackageContext("dev.endfield.modloader",Context.CONTEXT_IGNORE_SECURITY|Context.CONTEXT_INCLUDE_CODE);
                        String library=new java.io.File(module.getApplicationInfo().nativeLibraryDir,"libemodloader_host.so").getAbsolutePath();
                        Thread thread=Thread.currentThread();ClassLoader previous=thread.getContextClassLoader();
                        try{thread.setContextClassLoader(NativeBridge.class.getClassLoader());loadNative(library,gameLoader,gameApplication);}
                        finally{thread.setContextClassLoader(previous);}
                        if(!NativeBridge.start(index,"game"))throw new IllegalStateException("EML native start rejected");
                        loaded.set(true);
                        new Thread(()->{String applied=initial;while(true){try{Thread.sleep(1000);String next=settings.get().getString(ModuleStore.KEY,"");if(!next.equals(applied)&&!next.isEmpty()){String path=RuntimeMaterializer.prepare(context,next,source,text->android.util.Log.i("EModLoader",text));if(NativeBridge.update(OfflineHostService.read(path)))applied=next;}}catch(Exception error){android.util.Log.e("EModLoader","Index update failed",error);}}},"EML-Updates").start();
                    }catch(Throwable error){android.util.Log.e("EModLoader","Native load attempt "+attempts.get()+"/3 failed",error);}finally{loading.set(false);}
                }return result;});
            }
        }catch(Throwable error){android.util.Log.e("EModLoader","Game preparation failed",error);}
    }
    @android.annotation.SuppressLint("BlockedPrivateApi")
    private static void loadNative(String path,ClassLoader loader,Class<?> caller)throws ReflectiveOperationException {
        Method method;Object[] args;
        try{method=Runtime.class.getDeclaredMethod("nativeLoad",String.class,ClassLoader.class,Class.class);args=new Object[]{path,loader,caller};}
        catch(NoSuchMethodException absent){method=Runtime.class.getDeclaredMethod("nativeLoad",String.class,ClassLoader.class);args=new Object[]{path,loader};}
        method.setAccessible(true);Object error=method.invoke(null,args);if(error!=null)throw new UnsatisfiedLinkError(error.toString());
    }
}
