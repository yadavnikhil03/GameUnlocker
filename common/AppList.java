import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import java.lang.reflect.Method;
import java.util.List;

public class AppList {
    public static void main(String[] args) {
        try {
            Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            Method systemMainMethod = activityThreadClass.getMethod("systemMain");
            Object activityThread = systemMainMethod.invoke(null);
            
            Method getSystemContextMethod = activityThreadClass.getMethod("getSystemContext");
            Object context = getSystemContextMethod.invoke(activityThread);
            
            Class<?> contextClass = Class.forName("android.content.Context");
            Method getPackageManagerMethod = contextClass.getMethod("getPackageManager");
            PackageManager pm = (PackageManager) getPackageManagerMethod.invoke(context);
            
            List<ApplicationInfo> apps = pm.getInstalledApplications(PackageManager.GET_META_DATA);
            
            StringBuilder json = new StringBuilder("[\n");
            boolean first = true;
            
            for (ApplicationInfo app : apps) {
                if ((app.flags & ApplicationInfo.FLAG_SYSTEM) != 0 || (app.flags & ApplicationInfo.FLAG_UPDATED_SYSTEM_APP) != 0) {
                    continue;
                }
                
                String pkgName = app.packageName;
                CharSequence labelSeq = pm.getApplicationLabel(app);
                String label = (labelSeq != null) ? labelSeq.toString() : pkgName;
                
                label = label.replace("\"", "\\\"");
                
                if (!first) {
                    json.append(",\n");
                }
                json.append("  {\"package\": \"").append(pkgName).append("\", \"name\": \"").append(label).append("\"}");
                first = false;
            }
            json.append("\n]");
            System.out.println(json.toString());
            
        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
            System.exit(1);
        }
    }
}
