#ifndef __CDISABLEUPDATES__
#define __CDISABLEUPDATES__

#include <QScrollArea>

class CDisableUpdates {
public:
	inline CDisableUpdates(QWidget *w) : m_w(w), m_reenabled(false)
	{
		QScrollArea *sa = ::qobject_cast<QScrollArea *>(w);
		m_w = sa ? sa-> viewport() : w;
		m_upd_enabled = m_w->updatesEnabled();
		m_w->setUpdatesEnabled(false);
	}
	
	inline ~CDisableUpdates ( ) 
	{ 
		reenable(); 
	}
	
	inline void reenable()
	{
		if (!m_reenabled) {
			m_w->setUpdatesEnabled(m_upd_enabled);
			m_reenabled = true;
		}
	}

private:
	QWidget * m_w;
	bool      m_upd_enabled : 1;
	bool      m_reenabled   : 1;
};

#endif
