package anofax.opusrecord;

public class Native {
    public static native void initAudio();
    public static native void startRecorder( int sample_rate);
    public static native void stopRecorder();
    public static native int encode( String s, int sample_rate );
    public static native float volume();
    static {
        System.loadLibrary("OpusRecord");
        initAudio();
   }
}
