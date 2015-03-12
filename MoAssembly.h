#pragma once
#include "mobase.h"

class MoAssembly :
	public MoBase
{
public:
	MoAssembly(void);
	virtual ~MoAssembly(void);

	virtual bool write(FILE* moFile) const;
	virtual LPCTSTR baseName() const { return L"Assembly"; }

	void addBody(const MoBodyPtr& body) { m_bodies.push_back(body); }
	void addJoint(const MoJointPtr& joint) { m_joints.push_back(joint); }

	void thumbnail(const std::string& thumbnail) { m_thumbnail = thumbnail; }

private:
	void layout();

	std::vector<MoBodyPtr> m_bodies;
	std::vector<MoJointPtr> m_joints;

	std::string m_thumbnail;
};

