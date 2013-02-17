package anofax.opusrecord;

public class Native {
    public static native void initAudio();
    public static native void startRecorder();
    public static native void stopRecorder();
    public static native int encode( String s );
    static {
        System.loadLibrary("OpusRecord");
        initAudio();
   }
}
