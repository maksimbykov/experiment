#include "matrix.h"
#include <fstream>
#include <sstream>
#include <string>

Matrix::~Matrix()
{
	for(unsigned int counter=0; counter < m_data.size(); counter++ )
	{
		if ( 0 != m_data[counter] )
			delete m_data[counter];
	}

	m_data.clear();
}

int Matrix::fillFromFile(const char* _filename)
{
	std::fstream fs(_filename);
	std::string line;

	if ( fs.is_open() )
	{
		while( std::getline(fs, line) ) 
		{
			std::istringstream iss(line);
			int value = 0;
			
			m_data.push_back(new std::vector<int>());

			while ( iss >> value )
			{
				m_data.back()->push_back(value);
			}
		}

		return 0;
	}
	else
	{
		return 1;
	}
}