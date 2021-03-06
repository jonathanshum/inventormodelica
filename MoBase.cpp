#include "stdafx.h"
#include "MoBase.h"


MoBase::MoBase() :
	m_id(0),
	m_x(0.0),
	m_y(0.0),
	m_rotation(0.0),
	m_flipHorizontal(false),
	m_inDiagram(false)
{
}

MoBase::~MoBase(void)
{
}

void MoBase::name(const std::wstring& n)
{
	m_name = n;
	for (std::wstring::iterator i=m_name.begin(); i!=m_name.end(); ++i)
		if (*i == L' ')
			*i = L'_';
}

std::wstring MoBase::name() const
{
	TCHAR cstr[MAX_PATH];
	if (id() > 0)
	{
		_stprintf_s<MAX_PATH>(cstr, L"%s%d", m_name.empty() ? baseName() : m_name.c_str(), id());
		return cstr;
	}
	return m_name.empty() ? baseName() : m_name;
}

std::wstring MoBase::placement() const
{
	static double ext[] = {-10, -10, 10, 10};
	static double flip[] = {10, -10, -10, 10};

	TCHAR str[MAX_PATH];
	if (m_flipHorizontal)
		_stprintf_s<MAX_PATH>(str,L"Placement(visible = true, transformation(origin = {%f, %f}, extent = {{%f, %f}, {%f, %f}}, rotation = %f))", m_x, m_y, flip[0], flip[1], flip[2], flip[3], m_rotation);
	else
		_stprintf_s<MAX_PATH>(str,L"Placement(visible = true, transformation(origin = {%f, %f}, extent = {{%f, %f}, {%f, %f}}, rotation = %f))", m_x, m_y, ext[0], ext[1], ext[2], ext[3], m_rotation);

	return str;
}

bool MoBase::extendDiagram(double& xMax, double& yMin) const
{
	if (!inDiagram())
		return false;

	if (diagramX() > xMax)
		xMax = diagramX();

	if (diagramY() < yMin)
		yMin = diagramY();

	return true;
}
