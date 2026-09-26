// ---------------------------------------------------------------------------
// Provider kedua khusus proses ":UnityKillsMe" (MobaGameUnityActivity jalan di
// proses terpisah). Android hanya meng-instantiate provider di proses yang cocok
// dengan android:process-nya, DAN PMS mengindeks provider berdasarkan component
// name — dua <provider> dengan class yang sama membuat deklarasi kedua diabaikan.
// Karena itu dipakai subclass dengan nama class berbeda (authority juga beda).
// ---------------------------------------------------------------------------
package ph.over.mcgg;

public class MCGGProvider2 extends MCGGProvider {
    // seluruh perilaku diwarisi dari MCGGProvider (loadLibrary + MCGG.init)
}
