#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>


// --- DBSCAN ---
#define ETIKET_ZIYARET_EDILMEDI -99
#define ETIKET_GURULTU -1

// --- Line Yapısı ---
struct Line
{
    float a, b, c;       // ax + by + c = 0
    int inlier_sayac;    // Nokta sayısı
    float x_nokta[500]; // X koordinatları
    float y_nokta[500]; // Y koordinatları
    float uzunluk;       // Doğru uzunluğu (metre)
    float x1, y1, x2, y2;
};

void TomlREADER(const char *toml_url, float *angle_min, float *angle_max,
                float *angle_increment, float *range_min, float *range_max,
                float *ranges, int *range_sayac)
{
    char satir[2000];
    int inside_ranges = 0;

   char cmd[1024];
   sprintf(cmd, "curl -s -L -o temp_scan.toml %s", toml_url);
   system(cmd);

    FILE *f = fopen("temp_scan.toml", "r");
    if (!f)
    {
        printf("Dosya acilamadi!\n");
        return;
    }

    while (fgets(satir, sizeof(satir), f))
    {
        if (strstr(satir, "angle_min"))
            sscanf(satir, "angle_min = %f", angle_min);
        if (strstr(satir, "angle_max"))
            sscanf(satir, "angle_max = %f", angle_max);
        if (strstr(satir, "angle_increment"))
            sscanf(satir, "angle_increment = %f", angle_increment);
        if (strstr(satir, "range_min"))
            sscanf(satir, "range_min = %f", range_min);
        if (strstr(satir, "range_max"))
            sscanf(satir, "range_max = %f", range_max);

        if (strstr(satir, "ranges"))
            inside_ranges = 1;

        if (inside_ranges)
        {
            char *p = strchr(satir, '[');
            if (!p)
                p = satir;
            else
                p++;

            while (*p)
            {
                float val;
                if (sscanf(p, "%f", &val) == 1)
                    ranges[(*range_sayac)++] = val;
                while (*p && *p != ',' && *p != ']')
                    p++;
                if (*p == ',')
                    p++;
                if (*p == ']')
                {
                    inside_ranges = 0;
                    break;
                }
            }
        }
    }

    fclose(f);
    printf("%f %f %f %f %f\n", *range_max, *range_min, *angle_max, *angle_min, *angle_increment);
}

void kartezyenHesapla(float *ranges, int range_sayac, float range_min, float range_max,
                      int *kartezyen_sayac, float angle_min, float angle_increment,
                      float *x_konum, float *y_konum)
{
    for (int i = 0; i < range_sayac; i++)
    {
        float range = ranges[i];
        if (range < range_min || range > range_max)
            continue;

        float teta = angle_min + i * angle_increment;
        x_konum[*kartezyen_sayac] = range * cosf(teta);
        y_konum[*kartezyen_sayac] = range * sinf(teta);
        (*kartezyen_sayac)++;
    }
    printf("%d adet gecerli kartezyen nokta hesaplandi.\n", *kartezyen_sayac);
}

//////////////////////////// DBSCAN BASLA ////////////////////////////

float kareselMesafe(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

int bolgeSorgula(float *xDizi, float *yDizi, int noktaSayisi, int merkezIdx, float epsKare, int *komsuIndeksleri)
{
    int komsuSayisi = 0;
    for (int i = 0; i < noktaSayisi; i++)
        if (kareselMesafe(xDizi[merkezIdx], yDizi[merkezIdx], xDizi[i], yDizi[i]) <= epsKare)
            komsuIndeksleri[komsuSayisi++] = i;

    return komsuSayisi;
}

void kumeyiGenislet(float *xDizi, float *yDizi, int noktaSayisi, float epsKare, int minNokta,
                    int *etiketler, int *tohumIndeksleri, int tohumSayisi, int kumeId, int *gecici)
{
    int bas = 0, son = tohumSayisi;

    while (bas < son)
    {
        int p = tohumIndeksleri[bas++];

        if (etiketler[p] == ETIKET_GURULTU)
            etiketler[p] = kumeId;

        if (etiketler[p] != ETIKET_ZIYARET_EDILMEDI)
            continue;

        etiketler[p] = kumeId;

        int komsuSayisi = bolgeSorgula(xDizi, yDizi, noktaSayisi, p, epsKare, gecici);

        if (komsuSayisi >= minNokta)
        {
            for (int i = 0; i < komsuSayisi; i++)
            {
                int komsu = gecici[i];
                if (etiketler[komsu] == ETIKET_ZIYARET_EDILMEDI || etiketler[komsu] == ETIKET_GURULTU)
                    tohumIndeksleri[son++] = komsu;
            }
        }
    }
}

int dbscan(float *xDizi, float *yDizi, int noktaSayisi, float eps, int minNokta, int *etiketler)
{
    if (noktaSayisi <= 0)
        return 0;

    float epsKare = eps * eps;

    for (int i = 0; i < noktaSayisi; i++)
        etiketler[i] = ETIKET_ZIYARET_EDILMEDI;

    int *komsuIndeksleri = malloc(sizeof(int) * noktaSayisi);
    int *gecici = malloc(sizeof(int) * noktaSayisi);

    if (!komsuIndeksleri || !gecici)
        return 0;

    int kumeId = 0;

    for (int i = 0; i < noktaSayisi; i++)
    {
        if (etiketler[i] != ETIKET_ZIYARET_EDILMEDI)
            continue;

        int komsuSayisi = bolgeSorgula(xDizi, yDizi, noktaSayisi, i, epsKare, komsuIndeksleri);

        if (komsuSayisi < minNokta)
        {
            etiketler[i] = ETIKET_GURULTU;
            continue;
        }

        etiketler[i] = kumeId;

        memcpy(gecici, komsuIndeksleri, komsuSayisi * sizeof(int));

        kumeyiGenislet(xDizi, yDizi, noktaSayisi, epsKare, minNokta,
                        etiketler, gecici, komsuSayisi, kumeId, komsuIndeksleri);

        kumeId++;
    }

    free(komsuIndeksleri);
    free(gecici);

    printf("%d adet küme bulundu (DBSCAN).\n", kumeId);
    return kumeId;
}

//////////////////////////// DBSCAN SON ////////////////////////////

//////////////////////////// RANSAC BASLA ////////////////////////////

void dogruDenklemiHesapla(float x1, float y1, float x2, float y2, float *a, float *b, float *c)
{
    *a = y2 - y1;
    *b = -(x2 - x1);
    *c = (x2 - x1) * y1 - (y2 - y1) * x1;
    float norm = sqrt((*a) * (*a) + (*b) * (*b));
    if (norm > 1e-4)
    {
        *a /= norm;
        *b /= norm;
        *c /= norm;
    }
}

float noktaDogruMesafe(float x, float y, float a, float b, float c)
{
    return fabs(a * x + b * y + c);
}

int ransacDogruBul(float * x_konum, float * y_konum, int n,
                   float *best_a, float *best_b, float *best_c,
                   int *inlier_indices)
{
    if (n < 8)
        return 0;

    const int max_iterasyon = 500;
    const float esik_mesafe = 0.1;
    int en_iyi_inlier_sayisi = 0, en_iyi_inlier_indices[500];
    float en_iyi_a, en_iyi_b, en_iyi_c;

    for (int iter = 0; iter < max_iterasyon; iter++)
    {
        int idx1 = rand() % n, idx2 = rand() % n;
        while (idx2 == idx1)
            idx2 = rand() % n;
        float x1 =  x_konum[idx1], y1 =  y_konum[idx1];
        float x2 =  x_konum[idx2], y2 =  y_konum[idx2];
        if (hypot(x2 - x1, y2 - y1) < 0.01)
            continue;

        float a, b, c;
        dogruDenklemiHesapla(x1, y1, x2, y2, &a, &b, &c);

        int inlier_sayisi = 0,  gecici_inlier_indices[500];
        for (int i = 0; i < n; i++)
            if (noktaDogruMesafe( x_konum[i],  y_konum[i], a, b, c) < esik_mesafe)
                 gecici_inlier_indices[inlier_sayisi++] = i;

        if (inlier_sayisi > en_iyi_inlier_sayisi)
        {
            en_iyi_inlier_sayisi = inlier_sayisi;
            en_iyi_a = a;
            en_iyi_b = b;
            en_iyi_c = c;
            memcpy(en_iyi_inlier_indices,  gecici_inlier_indices, inlier_sayisi * sizeof(int));
        }
    }

    if (en_iyi_inlier_sayisi >= 8)
    {
        *best_a = en_iyi_a;
        *best_b = en_iyi_b;
        *best_c = en_iyi_c;
        memcpy(inlier_indices, en_iyi_inlier_indices, en_iyi_inlier_sayisi * sizeof(int));
        printf("    [RANSAC] En iyi model: %d inlier bulundu\n", en_iyi_inlier_sayisi);
        return en_iyi_inlier_sayisi;
    }
    return 0;
}

void kumedenDogruCikar(float *x_konum, float *y_konum, int kartezyen_sayac,
                       int  kume_id, int * kume_etiket,
                       struct Line *lines, int *line_sayac)
{
    float x_gecici[500], y_gecici[500];
    int original_indices[500];
    int n = 0;

    for (int i = 0; i < kartezyen_sayac; i++)
    {
        if ( kume_etiket[i] ==  kume_id)
        {
            x_gecici[n] = x_konum[i];
            y_gecici[n] = y_konum[i];
            original_indices[n] = i;
            n++;
        }
    }

    if (n < 8)
        return; 

    int kullanilmayan[500]; 
    for (int i = 0; i < n; i++)
    {
        kullanilmayan[i] = 1; // Başta hiçbir nokta kullanılmamış
    }

    int kalan_nokta_sayisi = n;

    while (kalan_nokta_sayisi >= 8)
    { 
        
        float x_kalan[500], y_kalan[500];
        int eski_index[500];
        int kalan_idx = 0;

        for (int i = 0; i < n; i++)
        {
            if (kullanilmayan[i])
            {
                x_kalan[kalan_idx] = x_gecici[i];
                y_kalan[kalan_idx] = y_gecici[i];
                eski_index[kalan_idx] = i;
                kalan_idx++;
            }
        }

        printf("  Kalan nokta sayisi: %d\n", kalan_nokta_sayisi);

        float a, b, c;
        int inlier_indices[500];
        int inlier_sayac = ransacDogruBul(x_kalan, y_kalan, kalan_nokta_sayisi,
                                          &a, &b, &c, inlier_indices);

        if (inlier_sayac == 0)
        {
            printf("  Daha fazla dogru bulunamadi.\n");
            break; 
        }

        // Bulunan doğruyu kaydet
        lines[*line_sayac].a = a;
        lines[*line_sayac].b = b;
        lines[*line_sayac].c = c;
        lines[*line_sayac].inlier_sayac = inlier_sayac;

        // İnlier noktaları kaydet ve işaretle
        float minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        for (int i = 0; i < inlier_sayac; i++)
        {
            int idx = eski_index[inlier_indices[i]];
            lines[*line_sayac].x_nokta[i] = x_gecici[idx];
            lines[*line_sayac].y_nokta[i] = y_gecici[idx];
            kullanilmayan[idx] = 0; // Bu nokta artık kullanildi.
            kalan_nokta_sayisi--;

            // Min/Max bul (uzunluk hesabı için)
            if (x_gecici[idx] < minX)
                minX = x_gecici[idx];
            if (x_gecici[idx] > maxX)
                maxX = x_gecici[idx];
            if (y_gecici[idx] < minY)
                minY = y_gecici[idx];
            if (y_gecici[idx] > maxY)
                maxY = y_gecici[idx];
        }

        // Doğrunun uzunluğunu hesapla (en uzak iki nokta arası mesafe)
        float dx = maxX - minX;
        float dy = maxY - minY;
        lines[*line_sayac].uzunluk = sqrt(dx * dx + dy * dy);

        printf("  Dogru #%d bulundu: %d nokta, uzunluk: %.2fm\n",
               *line_sayac + 1, inlier_sayac, lines[*line_sayac].uzunluk);

        (*line_sayac)++;

        if (*line_sayac >= 100)
            break; // Çok fazla doğru varsa dur
    }
}

//////////////////////////// RANSAC SON ////////////////////////////


int segmentKesisim(float x1, float y1, float x2, float y2,
                   float x3, float y3, float x4, float y4,
                   float *ix, float *iy)
{
    float s1_x = x2 - x1;
    float s1_y = y2 - y1;
    float s2_x = x4 - x3;
    float s2_y = y4 - y3;

    float det = (-s2_x * s1_y + s1_x * s2_y);
    if (fabs(det) < 1e-6)
        return 0; // Paralellik durumu olabilir.

    float s = (-s1_y * (x1 - x3) + s1_x * (y1 - y3)) / det;
    float t = (s2_x * (y1 - y3) - s2_y * (x1 - x3)) / det;

    if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
    {
        *ix = x1 + (t * s1_x);
        *iy = y1 + (t * s1_y);
        return 1;
    }

    return 0; 
}

int kumeNoktaSayisi(int * kume_etiket, int kartezyen_sayac, int  kume_id)
{
    int sayac = 0;
    for (int i = 0; i < kartezyen_sayac; i++)
    {
        if ( kume_etiket[i] ==  kume_id)
            sayac++;
    }
    return sayac;
}

float dogrularArasiAci(struct Line l1, struct Line l2)
{
    // Doğruların yön vektörleri
    float v1x = l1.x2 - l1.x1;
    float v1y = l1.y2 - l1.y1;
    float v2x = l2.x2 - l2.x1;
    float v2y = l2.y2 - l2.y1;

    float dot = v1x * v2x + v1y * v2y;
    float mag1 = sqrt(v1x * v1x + v1y * v1y);
    float mag2 = sqrt(v2x * v2x + v2y * v2y);

    if (mag1 < 1e-6 || mag2 < 1e-6)
        return 0.0;

    float cosTheta = dot / (mag1 * mag2);
    if (cosTheta > 1.0)
        cosTheta = 1.0;
    if (cosTheta < -1.0)
        cosTheta = -1.0;

    float aci = acos(cosTheta) * 180.0 / 3.14;

    float digerAci = 180.0 - aci;
    float minAci = fmin(aci, digerAci);

    return minAci; 
}

///////////////////////// DOSYAYA YAZDIRMA BASLA /////////////////////////

void verileriDosyayaYaz(float *x_konum, float *y_konum, int kartezyen_sayac)
{
    FILE *dosya = fopen("koordinatlar.dat", "w");
    if (dosya == NULL)
    {
        printf("koordinatlar.dat dosyasi olusturulamadi!\n");
        return;
    }

    printf("%d adet koordinat 'koordinatlar.dat' dosyasina yaziliyor...\n", kartezyen_sayac);
    for (int i = 0; i < kartezyen_sayac; i++)
    {
        fprintf(dosya, "%f %f\n", x_konum[i], y_konum[i]);
    }

    fclose(dosya);
    printf("Yazma islemi tamamlandi.\n");
}

void dogrulariDosyayaYaz(struct Line *lines, int line_sayac)
{
    
    for (int i = 0; i < line_sayac; i++)
    {
        char filename[50];
        sprintf(filename, "d%d.dat", i + 1);

        FILE *dosya = fopen(filename, "w");
        if (dosya == NULL)
        {
            printf("%s dosyasi olusturulamadi!\n", filename);
            continue;
        }

        for (int j = 0; j < lines[i].inlier_sayac; j++)
        {
            fprintf(dosya, "%f %f\n", lines[i].x_nokta[j], lines[i].y_nokta[j]);
        }

        fclose(dosya);
    }

    printf("%d dogru ayri dosyalara (d1.dat, d2.dat, ...) yazildi.\n", line_sayac);
}

void dogruCizgileriDosyayaYaz(struct Line *lines, int line_sayac)
{
    // Her doğru için ayrı line dosyası oluşturur.
    for (int i = 0; i < line_sayac; i++)
    {
        if (lines[i].inlier_sayac < 2)
            continue;

        char filename[50];
        sprintf(filename, "d%d_dogru.dat", i + 1);

        FILE *dosya = fopen(filename, "w");
        if (dosya == NULL)
        {
            printf("%s dosyasi olusturulamadi!\n", filename);
            continue;
        }

        // Doğruya ait noktaların uç noktalarını bulan yapıdır.
        float minX = lines[i].x_nokta[0], maxX = lines[i].x_nokta[0];
        float minY = lines[i].y_nokta[0], maxY = lines[i].y_nokta[0];
        int minXidx = 0, maxXidx = 0, minYidx = 0, maxYidx = 0;

        for (int j = 1; j < lines[i].inlier_sayac; j++)
        {
            if (lines[i].x_nokta[j] < minX)
            {
                minX = lines[i].x_nokta[j];
                minXidx = j;
            }
            if (lines[i].x_nokta[j] > maxX)
            {
                maxX = lines[i].x_nokta[j];
                maxXidx = j;
            }
            if (lines[i].y_nokta[j] < minY)
            {
                minY = lines[i].y_nokta[j];
                minYidx = j;
            }
            if (lines[i].y_nokta[j] > maxY)
            {
                maxY = lines[i].y_nokta[j];
                maxYidx = j;
            }
        }

     
        float xRange = maxX - minX;
        float yRange = maxY - minY;

        float x1, y1, x2, y2;
        if (xRange > yRange)
        {
            x1 = lines[i].x_nokta[minXidx];
            y1 = lines[i].y_nokta[minXidx];
            x2 = lines[i].x_nokta[maxXidx];
            y2 = lines[i].y_nokta[maxXidx];
        }
        else
        {
            x1 = lines[i].x_nokta[minYidx];
            y1 = lines[i].y_nokta[minYidx];
            x2 = lines[i].x_nokta[maxYidx];
            y2 = lines[i].y_nokta[maxYidx];
        }

       //Noktalarınıza ve doğrularınıza göre en uygun uzunluğu buradan ayarlayabilirsiniz.
        float dx = x2 - x1;
        float dy = y2 - y1;
        x1 -= dx * 1.9; 
        y1 -= dy * 1.9; 
        x2 += dx * 1.9; 
        y2 += dy * 1.9; 
        //////////////////////

        lines[i].x1 = x1;
        lines[i].y1 = y1;
        lines[i].x2 = x2;
        lines[i].y2 = y2;

        fprintf(dosya, "%f %f\n", x1, y1);
        fprintf(dosya, "%f %f\n", x2, y2);

        fclose(dosya);
    }

    printf("%d dogru cizgisi ayri dosyalara (d1_dogru.dat, ...) yazildi.\n", line_sayac);
}

void kesisimleriBulVeYaz(struct Line *lines, int line_sayac)
{
    FILE *dosya = fopen("kesisimler.dat", "w");
    FILE *onemliDosya = fopen("secilmis_kesisimler.dat", "w");

    if (!dosya || !onemliDosya)
    {
        printf("intersection dosyalari olusturulamadi!\n");
        return;
    }

    int toplam = 0, onemli_sayac = 0;
    for (int i = 0; i < line_sayac; i++)
    {
        for (int j = i + 1; j < line_sayac; j++)
        {
            float ix, iy;
            if (segmentKesisim(lines[i].x1, lines[i].y1, lines[i].x2, lines[i].y2,
                               lines[j].x1, lines[j].y1, lines[j].x2, lines[j].y2,
                               &ix, &iy))
            {
                fprintf(dosya, "%f %f\n", ix, iy);
                toplam++;

                float aci = dogrularArasiAci(lines[i], lines[j]);
                if (aci > 60.0) // Sınır açısını biz 60 derece belirledik, siz kendinize göre değiştirebilirsiniz.
                {
                    float uzaklik = sqrt(ix * ix + iy * iy); 
                    fprintf(onemliDosya, "%f %f %f %f %d %d\n", ix, iy, aci, uzaklik, i + 1, j + 1); 
                    onemli_sayac++;
                }
            }
        }
    }

    fclose(dosya);
    fclose(onemliDosya);

    printf("%d adet dogru kesisimi bulundu.\n", toplam);
    printf("%d tanesi 60 dereceden buyuk aciya sahip ve secilmis_kesisimler.dat dosyasina yazildi.\n", onemli_sayac);
}

///////////////////////// DOSYAYA YAZDIRMA SON /////////////////////////

void grafikCiz(struct Line *lines, int line_sayac)
{
    FILE *gnuplotPipe = popen("gnuplot -persistent", "w");

    if (gnuplotPipe)
    {
        fprintf(gnuplotPipe, "set title 'LIDAR Verisi - RANSAC ile Dogru Tespiti'\n");
        fprintf(gnuplotPipe, "set xlabel 'X Ekseni (metre)'\n");
        fprintf(gnuplotPipe, "set ylabel 'Y Ekseni (metre)'\n");
        fprintf(gnuplotPipe, "set grid\n");
        fprintf(gnuplotPipe, "set key outside right top\n");
        fprintf(gnuplotPipe, "set size ratio -1\n");
        fprintf(gnuplotPipe, "set xrange [-3.5:3.5]\n");
        fprintf(gnuplotPipe, "set yrange [-3.5:3.5]\n");

        fprintf(gnuplotPipe, "plot 'koordinatlar.dat' with points pt 7 ps 0.2 lc rgb '#b3d9ff' title 'Tum Lidar Noktalari'");

        for (int i = 0; i < line_sayac; i++)
        {
            fprintf(gnuplotPipe, ", \\\n     'd%d.dat' with points pt 7 ps 0.3 lc rgb 'green' title 'd%d (%d nokta)'", i + 1, i + 1, lines[i].inlier_sayac);
                            fprintf(gnuplotPipe, ", \\\n     'd%d.dat' with points pt 6 ps 0.5 lc rgb 'black' notitle",
                        i + 1);
            fprintf(gnuplotPipe, ", \\\n     'd%d_dogru.dat' with lines lw 3 lc rgb 'red' title 'd%d (%.2fm)'", i + 1, i + 1, lines[i].uzunluk);
        }

        fprintf(gnuplotPipe, ", \\\n     'secilmis_kesisimler.dat' with points pt 2 ps 1.5 lw 2.5 lc rgb 'orange' title '60+ Kesisimler'");
        fprintf(gnuplotPipe, ", \\\n     'secilmis_kesisimler.dat' using (0):(0):1:2 with vectors nohead lc rgb 'orange' dt 2 title 'Uzakliklar'");

        fprintf(gnuplotPipe, ", \\\n     'secilmis_kesisimler.dat' using 1:2:(sprintf(\"%%.0f°, (d%%dnd%%d), %%.2fm\", $3, $5, $6, $4)) with labels offset 0,0.15 font \",7\" tc rgb \"black\" notitle");

        fprintf(gnuplotPipe, ", \\\n     '-' with points pt 7 ps 1.5 lc rgb 'red' title 'Robot'\n");
        fprintf(gnuplotPipe, "0 0\n");
        fprintf(gnuplotPipe, "e\n");

        fflush(gnuplotPipe);
        fprintf(gnuplotPipe, "exit\n");
        pclose(gnuplotPipe);
        printf("Grafik olusturuldu.\n");
    }
    else
    {
        printf("Gnuplot bulunamadi veya calistirilamadi.\n");
    }
}

int main()
{
    printf("Program basladi\n");
    srand(time(NULL));

    const float dbscan_eps = 0.08;  
    const int dbscan_minpts = 3;    
    const int min_nokta_sayisi = 3; 

    float angle_max, angle_min, angle_increment, range_max, range_min;
    float x_konum[1000], y_konum[1000];
    float ranges[1000];
    int range_sayac = 0, kartezyen_sayac = 0;

    int  kume_etiket[1000];
    int  kume_sayac = 0;

    struct Line lines[100]; 
    int line_sayac = 0;

    char *url = ""; //Okunacak olan .toml uzantılı dosyanın linkini buraya yapıştırınız. 

    printf("\n=== 1. TOML Dosyasini Okuma ===\n");
    TomlREADER(url ,&angle_min, &angle_max, &angle_increment, &range_min, &range_max, ranges, &range_sayac);

    printf("\n=== 2. Kartezyen Koordinat Hesaplama ===\n");
    kartezyenHesapla(ranges, range_sayac, range_min, range_max, &kartezyen_sayac, angle_min, angle_increment, x_konum, y_konum);

    printf("\n=== 3. Koordinatlari Dosyaya Yazma ===\n");
    verileriDosyayaYaz(x_konum, y_konum, kartezyen_sayac);

    printf("\n=== 4. DBSCAN ile Kumelere Ayirma ===\n");
     kume_sayac = dbscan(x_konum, y_konum, kartezyen_sayac, dbscan_eps, dbscan_minpts,  kume_etiket);

    printf("\n=== 5. RANSAC ile Dogrulari Bulma ===\n");
    for (int i = 0; i <  kume_sayac; i++)
    {
        int nokta_sayisi = kumeNoktaSayisi( kume_etiket, kartezyen_sayac, i);

        if (nokta_sayisi >= min_nokta_sayisi)
        {
            printf("\nKume #%d (%d nokta) isleniyor:\n", i + 1, nokta_sayisi);
            kumedenDogruCikar(x_konum, y_konum, kartezyen_sayac, i,  kume_etiket, lines, &line_sayac);
        }
        else
        {
            printf("Kume #%d (%d nokta) - atlanıyor\n", i + 1, nokta_sayisi);
        }
    }

    printf("\n=== 6. Toplam %d dogru bulundu ===\n", line_sayac);

    printf("\n=== 7. Dogrulari Dosyaya Yazma ===\n");
    if (line_sayac > 0)
    {
        dogrulariDosyayaYaz(lines, line_sayac);
        dogruCizgileriDosyayaYaz(lines, line_sayac);
    }

    printf("\n=== 8. Grafik Olusturma ===\n");
    kesisimleriBulVeYaz(lines, line_sayac);
    grafikCiz(lines, line_sayac);

    printf("\nProgram bitti.\n");
    return 0;
}
