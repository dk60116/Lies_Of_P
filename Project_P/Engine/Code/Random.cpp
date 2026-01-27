#include "epch.h"
#include "Random.h"

CRandom::CRandom()
{
}

CRandom::~CRandom()
{
}

CRandom& CRandom::GetInstance()
{
	static CRandom inst;
	return inst;
}

HRESULT CRandom::Initialize()
{
	random_device rd;
	m_rng.seed(rd());

	return S_OK;
}

int CRandom::Range(const _int _min, const _int _max)
{
	uniform_int_distribution<int> dist(_min, _max - 1);
	return dist(m_rng);
}

float CRandom::Range(const _float _min, const _float _max)
{
	uniform_real_distribution<float> dist(_min, _max);
	return dist(m_rng);
}
