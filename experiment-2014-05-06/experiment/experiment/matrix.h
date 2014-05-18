#ifndef MATRIX_H
#define MATRIX_H

#include <vector>

/*we assume that matrix is square*/
class Matrix
{
	std::vector<std::vector<int>* > m_data;
public:
	Matrix(){};
	~Matrix();

	bool empty()           const { return m_data.empty(); }
	int  size()            const { return m_data.size(); } 
	int  get(int i, int j) const { return m_data[i]->at(j); }

	int fillFromFile(const char* _filename);
};

#endif //MATRIX_H