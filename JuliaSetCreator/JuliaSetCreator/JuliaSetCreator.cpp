// JuliaSetCreator.cpp // Herbie Warner
// Comments by me


#include <iostream>
#include <complex>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <cmath>


const char* outputFileName = "output.bmp"; //File name of output

//Color setting for output. Adjust according to preference. 
const double exponent = 0.9;
const double constant = 0.5;
const double scale = 0.1;




using std::complex; //For Convenience


//Some choices for c to generate different JuliaSets. There are others available. Generation via iterative formula z^2 = z^2 + c
complex<double> c(-0.79, 0.15); // Julia set 1.
//complex<double> c(-0.162, 1.04); // Julia set 2.
//complex<double> c(-0.4, 0.6); // Julia set 3.
//complex<double> c(-0.7269, 0.1889); // Julia set 4.


const int Width = 3000; //Number of pixels in x
const double aspectRatio = static_cast<double>(4) / 3;
const int Height = Width * 3 / 4;

double x = 0; //Central x
double y = 0; //Central y
const double xRange = 3; //Total range of x plane included
const double yRange = xRange / aspectRatio;
const int precision = 200; //Minimum number of iterations to pass to be considered within the set

const double minX = x - xRange / 2;
const double maxX = x + xRange / 2;
const double minY = y - yRange / 2;
const double maxY = y + yRange / 2;

const int tolerance = 2; //Minimum magnitude to exceed to be considered out of the set


std::mutex process; //Mutex for multiplie thread access to the imageMatrix



typedef std::chrono::steady_clock time_taken; //Clock definition for time

uint64_t image[Height][Width]; // Image data (0xRRGGBB).


void saveImage(const char* filename) {
	/*
	It is possible to output into a .png and acquire a higher resolution but I have not pursued this here. 
	The definitions of the BMP declaration are standard procedure.
	*/

	std::ofstream outfile(filename, std::ios::binary);

	// BMP file header
	uint8_t bmpHeader[54] = {
		'B', 'M',               // Signature
		0, 0, 0, 0,             // File size 
		0, 0, 0, 0,             // Reserved
		54, 0, 0, 0,            // Offset to pixel data
		40, 0, 0, 0,            // DIB header size
		Width & 0xff, (Width >> 8) & 0xff, (Width >> 16) & 0xff, (Width >> 24) & 0xff, // Image width
		Height & 0xff, (Height >> 8) & 0xff, (Height >> 16) & 0xff, (Height >> 24) & 0xff, // Image height
		1, 0,                   // Planes (must be 1)
		24, 0,                  // Bits per pixel (24-bit RGB)
		0, 0, 0, 0,             // Compression (no compression)
		0, 0, 0, 0,             // Image size (can be set to 0 for no compression)
		0, 0, 0, 0,             // X pixels per meter 
		0, 0, 0, 0,             // Y pixels per meter 
		0, 0, 0, 0,             // Colors in color table (0 for no color table)
		0, 0, 0, 0              // Important color count (0 means all colors are important)
	};

	// Calculate the file size
	uint32_t fileSize = sizeof(bmpHeader) + Width * Height * 3;
	bmpHeader[2] = fileSize & 0xff;
	bmpHeader[3] = (fileSize >> 8) & 0xff;
	bmpHeader[4] = (fileSize >> 16) & 0xff;
	bmpHeader[5] = (fileSize >> 24) & 0xff;

	outfile.write((const char*)bmpHeader, sizeof(bmpHeader));

	// Write pixel data directly to the file
	for (int y = Height - 1; y >= 0; --y) {
		for (int x = 0; x < Width; ++x) {
			outfile.write(reinterpret_cast<const char*>(&image[y][x]), 3);
		}
		// Pad rows to a multiple of 4 bytes (required for BMP)
		for (int p = 0; p < (4 - (Width * 3) % 4) % 4; ++p) {
			outfile.put(0);
		}
	}
	outfile.close();
}


struct RGB {
	int r; // Red [0, 255]
	int g; // Green [0, 255]
	int b; // Blue [0, 255]
	RGB() = default;
};

RGB hsvToRgb(double hue, double saturation, double value) {
	//Convert hsv to RGB via standard formula
	double c = value * saturation;
	double x = c * (1 - std::abs(std::fmod(hue / 60.0, 2) - 1));
	double m = value - c;

	double r, g, b;

	if (hue >= 0 && hue < 60) {
		r = c;
		g = x;
		b = 0;
	}
	else if (hue >= 60 && hue < 120) {
		r = x;
		g = c;
		b = 0;
	}
	else if (hue >= 120 && hue < 180) {
		r = 0;
		g = c;
		b = x;
	}
	else if (hue >= 180 && hue < 240) {
		r = 0;
		g = x;
		b = c;
	}
	else if (hue >= 240 && hue < 300) {
		r = x;
		g = 0;
		b = c;
	}
	else {
		r = c;
		g = 0;
		b = x;
	}

	RGB rgb;
	rgb.r = static_cast<int>((r + m) * 255);
	rgb.g = static_cast<int>((g + m) * 255);
	rgb.b = static_cast<int>((b + m) * 255);

	return rgb;
}


RGB logColor(double distance, double base, double constant, double scale) {
	//Definition of color scheme for output using logarithms
	double color = -1 * log10(base) / log10(distance); //In HSV
	RGB rgb = hsvToRgb(constant + scale * color, 0.8, 0.9); // Defining s,v
	return rgb;

}

RGB powerColor(double distance) {
	//Definition of color scheme for output using exponential
	double color = std::pow(distance, exponent);
	double normalizedDistance = distance / scale;
	double hue = std::fmod(constant + normalizedDistance, 1.0);
	RGB rgb = hsvToRgb(hue * 360.0, 1 - 0.6 * color, 0.9);
	return rgb;
}



void computeJulia(size_t idx, size_t threads) {
	
	size_t startRow = (Height / threads) * idx; //Calculate part of complex plane this thread is responsible for.
	size_t endRow = (Height / threads) * (idx + 1);

	for (size_t row = startRow; row < endRow; ++row) {
		for (size_t column = 0; column < Width; ++column) {
			double x = minX + column * xRange / Width;
			double y = maxY - row * yRange / Height;

			complex<double> z(x, y);
			int iterations = 0;

			//Compute for each point in the complex plane.
			while (abs(z) <= tolerance && iterations < precision) {
				z = (z * z) + c; 
				++iterations;
			}

			if (iterations < precision) { //Point outside set and will have a colour dependent on how many iterations it took to leave the set

				double distance = static_cast<double>(iterations) / precision;
				RGB rgb = powerColor(distance); 

				process.lock(); //Lock access to the image data to assign colour
				image[row][column] = (rgb.r << 16) | (rgb.g << 8) | (rgb.b); //Assign colours
				process.unlock(); //Unlock mutex
			}
		}
	}
}

int main()
{
	std::cout << "Generating Julia Set..." << std::endl; // Start timing the process of making the Julia Set

	time_taken::time_point start = time_taken::now();

	//Initiate threading of tasks
	const int numThreads = std::thread::hardware_concurrency(); //Uses all available threads to maximum speed.
	std::cout << "Thread capacity: " << numThreads << std::endl;
	std::vector<std::thread> threads;
	for (size_t idx = 0; idx < numThreads; ++idx) { //Assign tasks to each thread for different parts of the complex plane
		std::cout << "Launched thread:" << idx << std::endl;
		threads.emplace_back([idx, numThreads]() {
			computeJulia(idx, numThreads);
			});
	}

	//Join the threads
	for (std::thread& thread : threads) {
		thread.join();
	}

	time_taken::time_point end = time_taken::now();
	auto time_taken = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
	std::cout << "Computing the Julia Set took " << time_taken << " s." << std::endl;
	saveImage(outputFileName);
	return 0;
}
