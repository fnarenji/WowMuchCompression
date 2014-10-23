#include "stdafx.h"

using namespace nsWMC;
using namespace std;

CCompressor::CCompressor(const std::string& inputFilePath, const std::string& outputFilePath)
	: m_inputFilePath(inputFilePath), m_outputFilePath(outputFilePath)
{

}

void CCompressor::run() const
{
	/**
	  * CRawVideoLoader est une classe nous permettant d'importer des fichiers vid�os.
	  * Elle se base sur OpenCV, une biblioth�que qui permet entre autre la manipulation de fichiers vid�os (image uniquement).
	  * Ici, la classe va tenter d'ouvrir le fichier original.avi. Si une erreur se produit (droits d'acc�s insuffisants, fichier inexistant...)
	  * une exception (erreur) sera lev�e.
	  *
	  **/
	CRawVideoLoader rawVideoLoader(m_inputFilePath);

	/**
	  * La classe CComponentFrame est une classe qui permet de stocker toutes les valeurs d'une composante pour une image.
	  * Par exemple, toutes les valeurs de Y pour une image I. Elle fait donc office de matrice pour une composante image enti�re.
	  *
	  * Il y a 3 composantes - Y, Cb & Cr. On d�clare donc 3 CComponentFrame par image.
	  *
	  **/
	CComponentFrame currentYVideoFrame, currentCbVideoFrame, currentCrVideoFrame,
		prevYVideoFrame, prevCbVideoFrame, prevCrVideoFrame;

	/**
	  * La classe CSerializableComponentFrame repr�sente les m�mes donn�es que CComponentFrame
	  * mais sous la forme d'une suite de donn�es continue, plus simple � stocker dans des fichiers.
	  *
	  **/
	CSerializableComponentFrame serializableYVideoFrame, serializableCbVideoFrame, serializableCrVideoFrame;
	CComponentFrame matchesYVideoFrame, matchesCbVideoFrame, matchesCrVideoFrame,
		prevMatchesYVideoFrame, prevMatchesCbVideoFrame, prevMatchesCrVideoFrame;

	/**
	  * Les frames (images) du fichier vid�o sont import�es avec comme format de couleur RGB (Red Green Blue).
	  * Pour servir nos objectifs, nous convertissons le RGB en YCbCr (Y �tant la luminance, Cb et Cr �tant des valeurs de chrominance).
	  * La fonction GetYCbCrFrames va remplir nos vecteurs de CComponentFrame.
	  * Pour cela, elle va charger une frame depuis le fichier, convertir chaque pixel de RGB vers YCbCr, et les stocker en m�moire.
	  * On charge les deux premi�res frames du fichier.
	  *
	  **/
	if (!rawVideoLoader.GetNextYCbCrFrame(currentYVideoFrame, currentCbVideoFrame, currentCrVideoFrame))
		throw runtime_error("Couldn't load first frame.");

	/**
	  * CCompressedVideoWriter est une classe permettant d'exporter vers un fichier les donn�es une fois compr�ss�e.
	  * Les donn�es sont stock�es dans un format propre au codec WMC.
	  * Un fichier cr�er par CCompressedVideoWriter doit donc �tre lu avec CCompressedVideoReader.
	  * 
	  **/
	CCompressedVideoWriter compressedVideoWriter(m_outputFilePath,
						rawVideoLoader.GetWidth(), rawVideoLoader.GetHeight(),
						rawVideoLoader.GetWidthPadding(), rawVideoLoader.GetHeightPadding());

	/**
	  * CDiscreteCosineTransform est une classe qui applique la transform�e en cosinus discr�te sur un CComponentFrame.
	  * Elle accepte un premier param�tre bool�en, indiquant si la dct doit �tre inverse (iDCT).
	  * Elle accepte comme deuxi�me param�tre param�tre un bool�en, indiquant si une quantification doit avoir lieu.
	  * Elle accepte un trois�me param�tre entier, indiquant la qualit� en pourcentage apr�s la quantification
	  * par rapport � l'image originale.
	  *
	  **/
	CDiscreteCosineTransform dct(false, true);

	/**
	  * CChromaSubsampler est une classe qui sous �chantillone une composante.
	  * A terme, elle permet de ne stocker, pour 4 composantes Y, une seule composante Cb et une seule composante Cr.
	  * C'est une op�ration "avec perte".
	  *
	  **/
	CChromaSubsampler chromaSubsampler;

	/**
	  * CRunLengthEncoder une classe permettant de "factoriser" les suites de z�ros cons�cutifs.
	  * Sur une matrice DCT quantifi�e, plus on est loin de (0,0) (hautes fr�quences), plus les valeurs sont petites.
	  * Les valeurs proches de 0 sont arrondies � 0 lors de la quantification.
	  * Le RunLengthEncoder permet alors de factoriser une suite de z�ros dans la matrice en ne gardant qu'un seul z�ro
	  * suivi du nombre de z�ro qu'il y avait originalement.
	  *
	  **/
	CRunLengthEncoder runLengthEncoder;

	CBlockMatcher blockMatcher;
	
	// Pour chaque frame
	int i = 0;

	do
	{
		cout << i << ": processing frame." << endl;

		cout << "\tChroma subsampling on Cb component... ";
		chromaSubsampler(currentCbVideoFrame);

		cout << "Done." << endl << "\tChroma subsampling on Cr component... ";
		chromaSubsampler(currentCrVideoFrame);

		cout << "Done." << endl << "\tApplying discrete cosine transform on Y component... ";
		dct(currentYVideoFrame);

		cout << "Done." << endl << "\tApplying discrete cosine transform on Cb component... ";
		dct(currentCbVideoFrame);

		cout << "Done." << endl << "\tApplying discrete cosine transform on Cr component... ";
		dct(currentCrVideoFrame);
		
		if (i != 0) // first frame doesn't have previous frame
		{
			serializableYVideoFrame.resize(0);
			serializableCbVideoFrame.resize(0);
			serializableCrVideoFrame.resize(0);

			blockMatcher(prevYVideoFrame, currentYVideoFrame, serializableYVideoFrame, matchesYVideoFrame, prevMatchesYVideoFrame);
			blockMatcher(prevCbVideoFrame, currentCbVideoFrame, serializableCbVideoFrame, matchesCbVideoFrame, prevMatchesCbVideoFrame);
			blockMatcher(prevCrVideoFrame, currentCrVideoFrame, serializableCrVideoFrame, matchesCrVideoFrame, prevMatchesCrVideoFrame);
		}
		else // handle first frame case
		{
			serializableYVideoFrame.resize(currentYVideoFrame.size());
			serializableCbVideoFrame.resize(currentCbVideoFrame.size());
			serializableCrVideoFrame.resize(currentCrVideoFrame.size());

			matchesYVideoFrame.resize(currentYVideoFrame.rows() / 8, currentCbVideoFrame.cols() / 8);
			matchesCbVideoFrame.resize(currentCbVideoFrame.rows() / 8, currentCbVideoFrame.cols() / 8);
			matchesCrVideoFrame.resize(currentCrVideoFrame.rows() / 8, currentCrVideoFrame.cols() / 8);

			for (int k = 0; k < currentYVideoFrame.rows(); ++k)
				for (int n = 0; n < currentYVideoFrame.cols(); ++n)
					serializableYVideoFrame.push_back(currentYVideoFrame(k, n));

			for (int k = 0; k < currentCbVideoFrame.rows(); ++k)
				for (int n = 0; n < currentCbVideoFrame.cols(); ++n)
					serializableCbVideoFrame.push_back(currentCbVideoFrame(k, n));

			for (int k = 0; k < currentCrVideoFrame.rows(); ++k)
				for (int n = 0; n < currentCrVideoFrame.cols(); ++n)
					serializableCrVideoFrame.push_back(currentCrVideoFrame(k, n));
		}

		cout << "Done." << endl << "\tRun length encoding on Y component... ";
		runLengthEncoder(serializableYVideoFrame);

		cout << "Done." << endl << "\tRun length encoding on Cb component... ";
		runLengthEncoder(serializableCbVideoFrame);

		cout << "Done." << endl << "\tRun length encoding on Cr component... ";
		runLengthEncoder(serializableCrVideoFrame);

		cout << "Done." << endl << "\tSaving frame to file... ";
		compressedVideoWriter.SaveFrame(serializableYVideoFrame, serializableCbVideoFrame, serializableCrVideoFrame,
			matchesYVideoFrame, matchesCbVideoFrame, matchesCrVideoFrame);
		
		cout << "Done." << endl;

		prevYVideoFrame = currentYVideoFrame;
		prevCbVideoFrame = currentCbVideoFrame;
		prevCrVideoFrame = currentCrVideoFrame;
		prevMatchesYVideoFrame = matchesYVideoFrame;
		prevMatchesCbVideoFrame = matchesCbVideoFrame;
		prevMatchesCrVideoFrame = matchesCrVideoFrame;

		cout << endl;
		++i;
	} while (rawVideoLoader.GetNextYCbCrFrame(currentYVideoFrame, currentCbVideoFrame, currentCrVideoFrame));

	cout << "There are no more frames to be read." << endl;

	/**
	  * On cloture le fichier en inscrivant au d�but le nombre de frames �crites
	  * et en lib�rant les ressources syst�mes allou�es.
	  *
	  **/
	compressedVideoWriter.Finalize();
}
