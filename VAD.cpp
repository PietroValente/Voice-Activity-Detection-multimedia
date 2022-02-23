#include <iostream>
#include <vector>
#include <complex>
#include <valarray>
#include <map>
#include <numeric>

const double PI = 3.141592653589793238460;
 
using namespace std;

typedef complex<double> Complex;
typedef valarray<Complex> CArray;

char N;
 
class vad{

	int sample; //grandezza pacchetto
	int sample_window; //grandezza finestra che deve essere potenza di 2
	int sample_lack; //differenza di campioni tra il pacchetto e la finestra 
	int sample_start; //campione iniziale del pacchetto in considerazione
	int sample_end; //campione finale del pacchetto in considerazione
	int sample_freq; //frequenza di campionamento dei pacchetti

	double energy_threshold; //soglia di energia
	double speech_start_band; //frequenza iniziale dell'intervallo = picco - 50 Hz
	double speech_end_band; //frequenza finale dell'intervallo = picco + 50 Hz

	vector< double > amplitudeArrInitial; //vettore con le ampiezze iniziali (nel tempo)
	vector< double > amplitudeArrFinal; //vettore con le ampiezze finali (nel tempo)
	vector< char > samplesChoices; //vettore con le scelte di tutti i pacchetti

	//COSTRUTTORE
	public: vad(char *file)
	{
		read_data(file); //lettura file input

        sample = 160; //inizializzazione parametri
		sample_freq = 8000;
		sample_start = 0;
		sample_window = 256;
		energy_threshold = 6 * pow(10,7);
		sample_lack = sample_window-sample;

		detect_speech_VAD(); //chiamata metodo principale
		write_data(); //scrittura dei risultati ottenuti
	}

	//METODO PER LETTURA DATI INPUT
	void read_data(char *file){

		FILE *data = fopen(file, "rb");

    	while (!feof(data) && !ferror(data)) {

			int tmp1 = 0;
			unsigned char tmp2;
			fread(&tmp2, 1, 1, data);
			tmp1 = tmp2;
			double tmp3 = static_cast<double>(tmp1);
			amplitudeArrInitial.push_back(tmp3);
		}
    	fclose(data);
	}

	//METODO PER SCRITTURA DEI 2 FILE DI OUTPUT
	void write_data(){

		char nameFile1[30] = "outputaudio/outputaudioN.data"; //file .data con risultati VAD
		nameFile1[23] = N;
		FILE *data = fopen(nameFile1, "wb");

		char nameFile2[28] = "outputVAD/outputVADN.txt"; //file .txt con risultati VAD, scelta pacchetti
		nameFile2[19] = N;
		FILE *data2 = fopen(nameFile2, "wb");

		for (vector<double>::const_iterator i = amplitudeArrFinal.begin(); i != amplitudeArrFinal.end(); ++i){
			int tmp1 = static_cast<int>(*i);
			char tmp2 = static_cast<char>(tmp1);
			fwrite(&tmp2, 1, 1, data);
		}
    	fclose(data);

		for (vector<char>::const_iterator i = samplesChoices.begin(); i != samplesChoices.end(); ++i){
			char tmp1 = *i;
			fwrite(&tmp1, 1, 1, data2);
		}
		fclose(data2);
	}

	//METODO CHE SVOLGE L'ALGORITMO COMPLETO VAD
	void detect_speech_VAD(){

		bool lock = true; //lock che permette di analizzare la scelta del pacchetto precedente a prev
		bool prev = false; //pacchetto precedente a quello in considerazione
		bool now; //pacchetto in considerazione
		bool next; //pacchetto successivo a quello in considerazione
		bool nextNext; //pacchetto successivo al next

		bool choice;
		
		while (sample_start < (amplitudeArrInitial.size() - sample)){ //ultimo pacchetto con meno di 160 campioni viene scartato

			sample_end = sample_start + sample;

			if(sample_start < sample_lack){ //operazioni particolari per il primo pacchetto
				now = energy_sample_decision(vector<double>( amplitudeArrInitial.begin(), amplitudeArrInitial.begin() + sample_window)); //energy decision now
				next = energy_sample_decision(vector<double>(amplitudeArrInitial.begin() + sample_start + sample - sample_lack , amplitudeArrInitial.begin() + sample_start + 2 * sample)); //enenergy decision next
			}
			if(sample_start + 2*sample < amplitudeArrInitial.size()){ //operazione effettuata solo se ci sono 2 pacchetti successivi
				nextNext = energy_sample_decision(vector<double>(amplitudeArrInitial.begin() + sample_start + 2 * sample - sample_lack , amplitudeArrInitial.begin() + sample_start + 3 * sample)); //energy decision nextNext
			}
			else{ //operazione effettuata se non ci sono 2 pacchetti successivi
				nextNext = false;
			}

			if(now || next || nextNext){ //se uno tra now, next e nextNext ha energia sufficente comporta choice = true
				choice = true;
				prev = choice;
			}
			else if(prev){ //questo meccanismo mi permette di considerare prev e prevPrev senza cadere in un loop continuo che, 
				if(lock){  //una volta trovato un pacchetto con voce, terrebbe tutti i pacchetti successivi
					prev = choice;
					lock = false;
				}
				else{
					lock = true;
					prev = false;
				}
				choice = true;
			}
			else{
				choice = false;
				prev = choice;
			}

			if(choice){ //il pacchetto viene trasmesso
				samplesChoices.push_back('1'); //aggiungo la decisione del pacchetto
				for(int i = sample_start; i<sample_start+sample; i++){ //aggiungo tutti i campioni in esame
					amplitudeArrFinal.push_back(amplitudeArrInitial.at(i));
				}
			}
			else{ //il pacchetto non viene trasmesso
				samplesChoices.push_back('0'); //aggiungo la decisione del pacchetto
				for(int i = sample_start; i<sample_start+sample; i++){ //modifico tutti i campioni in esame a 0
					amplitudeArrFinal.push_back(0);
				}
			}
			now = next; //aggiorno le variabili
			next = nextNext;
			sample_start += sample;
		}
	}

	//METODO CHE SVOLGE L'ALGORITMO ENERGY DECISION
	bool energy_sample_decision(vector<double> data_window){
			
			map<double, double> energy_freq = calculate_normalized_energy(data_window); //calcolo dizionario con energia associata a frequenze
			double sum_voice_energy = sum_energy_in_band(energy_freq, speech_start_band, speech_end_band); //calcolo l'energia nell'intervallo in considerazione
			
			if(sum_voice_energy > energy_threshold){ //valutazione se energia superiore alla soglia oppure no
				return true;
			}
			else{
				return false;
			}
	}

	//METODO PER ASSOCIARE L'ENERGIA AD OGNI FREQUENZA
	map<double, double> calculate_normalized_energy(vector<double> data_window){

        vector<double> data_freq = calculate_frequencies(data_window); //vettore con le frequenze dei campioni del pacchetto
        vector<double> data_energy = calculate_amplitude_squared(data_window); //vettore con le energie dei campioni del pacchetto

        map<double, double> energy_freq = connect_energy_with_frequencies(data_freq, data_energy); //connetto le frequenze alle relative energie
        return energy_freq;
	}

	//METODO PER CALCOLARE LE FREQUENZE PRESENTI NEL PACCHETTO
	vector<double> calculate_frequencies(vector<double> data_window){

		vector<double> data_freq = fftfreq(data_window.size(),1.0/sample_freq); //calcolo le fftfreq

		data_freq.erase(data_freq.begin()); //tolgo la frequenza 0 Hz
		data_freq.resize(data_freq.size()/2); //divido in due il vettore perchè precedentemente va da [-fmax, fmax]
		return data_freq;
	}

	//METODO PER CALCOLARE LE FAST FOURIER TRANSFORM FREQUENCIES
	vector<double> fftfreq(int size,double spacing){

		vector<double> data_freq;
		data_freq.push_back(0);
		int middle = size/2;
		int i;
		for (i = 1; i < middle; i++) {
			double number = i/(size*spacing);
			data_freq.push_back(number);
		}
		if(size%2 == 1){
			double number = i/(size*spacing);
			data_freq.push_back(number);
		}
		for (; i > 0; i--) {
			double number = -i/(size*spacing);
			data_freq.push_back(number);
		}
		return data_freq;
	}

	//METODO PER CALCOLARE LE ENERGIE DELLE AMPIEZZE DEI CAMPIONI PRESENTI NEL PACCHETTO
	vector<double> calculate_amplitude_squared(vector<double> data_window){

		Complex C[data_window.size()]; //trasformo l'array con le ampiezze in un array di complessi con parte immaginaria a zero
		for(int i = 0; i < data_window.size(); i++){
			C[i].real(data_window[i]);
		}
		CArray data(C,data_window.size());
    	
		fft(data); //applico FFT

		vector<double> data_ampl = absAmplitudeSquared(data); //calcolo il quadrato dell'ampiezza che sarebbe l'energia

		data_ampl.erase(data_ampl.begin()); //tolgo l'energia del campione a 0 Hz
		data_ampl.resize(data_ampl.size()/2); //divido in due il vettore perchè precedentemente va da [-fmax, fmax]
		return data_ampl;
	}

	//METODO PER CALCOLARE LA FAST FOURIER TRANSFORM
	void fft(CArray &x){

		unsigned int N = x.size(), k = N, n;
		double thetaT = PI / N;
		Complex phiT = Complex(cos(thetaT), -sin(thetaT)), T;
		while (k > 1)
		{
			n = k;
			k >>= 1;
			phiT = phiT * phiT;
			T = 1.0L;
			for (unsigned int l = 0; l < k; l++)
			{
				for (unsigned int a = l; a < N; a += n)
				{
					unsigned int b = a + k;
					Complex t = x[a] - x[b];
					x[a] += x[b];
					x[b] = t * T;
				}
				T *= phiT;
			}
		}
		unsigned int m = (unsigned int)log2(N);
		for (unsigned int a = 0; a < N; a++)
		{
			unsigned int b = a;
			b = (((b & 0xaaaaaaaa) >> 1) | ((b & 0x55555555) << 1));
			b = (((b & 0xcccccccc) >> 2) | ((b & 0x33333333) << 2));
			b = (((b & 0xf0f0f0f0) >> 4) | ((b & 0x0f0f0f0f) << 4));
			b = (((b & 0xff00ff00) >> 8) | ((b & 0x00ff00ff) << 8));
			b = ((b >> 16) | (b << 16)) >> (32 - m);
			if (b > a)
			{
				Complex t = x[a];
				x[a] = x[b];
				x[b] = t;
			}
		}
	}

	//METODO PER CALCOLARE L'ENERGIA = AMPIEZZA ^ 2
	vector<double> absAmplitudeSquared(CArray x){

		vector<double> absAmplitude;
		for (int i = 0; i < x.size(); i++){
			absAmplitude.push_back(pow(abs(x[i]),2));
    	}
		return absAmplitude;
	}

	//METODO PER CONNETTERE LE FREQUENZE ALLE RELATIVE ENERGIE
	map<double, double> connect_energy_with_frequencies(vector<double> data_freq, vector<double> data_energy){

		map<double, double> energy_freq;
		if(data_freq.size() != data_energy.size()){ //se il vettore con le frequenze ha una dimensione diversa 
			cout << "Error" << '\n';				//da quello con le energie c'è un errore
		}

		double peak = 0; //picco
		double frequency = 0; //frequenza del picco

		for(int i = 0; i < data_freq.size(); i++){ //creazione del dizionario e viene trovato il picco
			energy_freq[data_freq[i]] = data_energy[i];
			if(data_energy[i] > peak && (data_freq[i]>100)){
				peak = data_energy[i];
				frequency = data_freq[i];
			}
		}

		speech_start_band = frequency - 50; //viene stabilito l'intervallo delle frequenze
		speech_end_band = frequency + 50;
		return energy_freq;
	}

	//METODO PER CALCOLARE L'ENERGIA DELL'INTERVALLO DI FREQUENZE
	double sum_energy_in_band(map<double, double> energy_frequencies, double start_band, double end_band){

		double sum_energy_in_band = 0;
		for (auto const& [key, val] : energy_frequencies){
			if( (start_band < key) && (key < end_band) ){
				sum_energy_in_band += val;
			}
		}
		return sum_energy_in_band;
	}
};


//MAIN
int main(){
	char nameFile[28] = "inputaudio/inputaudioN.data";
	cout << "Inserisci il numero N di inputaudioN.data:";
  	cin >> N;
	nameFile[21] = N;
	vad file = vad(nameFile);
}
