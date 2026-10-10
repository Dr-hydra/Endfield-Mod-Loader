package dev.endfield.modloader;
public final class LoaderApplication extends android.app.Application {
    @Override public void onCreate(){super.onCreate();if(!android.app.Application.getProcessName().endsWith(":offline")){FrameworkSettings.initialize(this);try{ModuleStore.ensure(this);}catch(Exception error){android.util.Log.e("EModLoader","Module index unavailable",error);}}}
}
