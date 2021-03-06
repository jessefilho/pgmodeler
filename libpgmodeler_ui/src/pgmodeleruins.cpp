#include "pgmodeleruins.h"
#include "messagebox.h"
#include "databasemodel.h"
#include <QLabel>
#include "numberedtexteditor.h"

namespace PgModelerUiNS {

	NumberedTextEditor *createNumberedTextEditor(QWidget *parent)
	{
		NumberedTextEditor *editor=new NumberedTextEditor(parent);

		if(parent && !parent->layout())
		{
			QHBoxLayout *layout=new QHBoxLayout(parent);
			layout->setContentsMargins(0,0,0,0);
			layout->addWidget(editor);
		}

		return(editor);
	}

	QTreeWidgetItem *createOutputTreeItem(QTreeWidget *output_trw, const QString &text, const QPixmap &ico, QTreeWidgetItem *parent, bool expand_item, bool word_wrap)
	{
		if(!output_trw)
			throw Exception(ERR_OPR_NOT_ALOC_OBJECT,__PRETTY_FUNCTION__,__FILE__,__LINE__);

		QTreeWidgetItem *item=nullptr;

		item=new QTreeWidgetItem(parent);
		item->setIcon(0, ico);

		if(!parent)
			output_trw->insertTopLevelItem(output_trw->topLevelItemCount(), item);

		if(!word_wrap)
			item->setText(0, text);
		else
		{
			QLabel *label=new QLabel;
			label->setTextFormat(Qt::AutoText);
			label->setText(text);
			label->setWordWrap(true);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse);
			label->setMinimumHeight(output_trw->iconSize().height());
			label->setMaximumHeight(label->heightForWidth(label->width()));
			output_trw->setItemWidget(item, 0, label);
		}

		item->setFlags(Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		item->setExpanded(expand_item);
		output_trw->setItemHidden(item, false);
		output_trw->scrollToBottom();

		return(item);
	}

	void createOutputListItem(QListWidget *output_lst, const QString &text, const QPixmap &ico, bool is_formated)
	{
		if(!output_lst)
			throw Exception(ERR_OPR_NOT_ALOC_OBJECT,__PRETTY_FUNCTION__,__FILE__,__LINE__);

		QListWidgetItem *item=new QListWidgetItem;
		item->setIcon(ico);
		output_lst->addItem(item);

		if(!is_formated)
			item->setText(text);
		else
		{
			QLabel *label=new QLabel(text);
			output_lst->setItemWidget(item, label);
		}
	}

	void disableObjectSQL(BaseObject *object, bool disable)
	{
		if(object && object->getObjectType()!=BASE_RELATIONSHIP)
		{
			Messagebox msgbox;
			ObjectType obj_type=object->getObjectType();
			bool curr_val=object->isSQLDisabled();

			if(object->isSystemObject())
				throw Exception(Exception::getErrorMessage(ERR_OPR_RESERVED_OBJECT)
								.arg(object->getName(true))
								.arg(object->getTypeName()),
								ERR_OPR_RESERVED_OBJECT,__PRETTY_FUNCTION__,__FILE__,__LINE__);

			object->setSQLDisabled(disable);

			if(obj_type!=OBJ_DATABASE && curr_val!=disable)
			{
				msgbox.show(QString(QT_TR_NOOP("Do you want to apply the <strong>SQL %1 status</strong> to the object's references too? This will avoid problems when exporting or validating the model.")).arg(disable ? QT_TR_NOOP("disabling") : QT_TR_NOOP("enabling")),
							Messagebox::CONFIRM_ICON, Messagebox::YES_NO_BUTTONS);

				if(msgbox.result()==QDialog::Accepted)
					disableReferencesSQL(object);
			}

			/* Special case for tables. When disable the code there is the need to disable constraints
	   codes when the code of parent table is disabled too in order to avoid export errors */
			if(object->getObjectType()==OBJ_TABLE)
			{
				Constraint *constr = nullptr;
				vector<TableObject *> *objects=dynamic_cast<Table *>(object)->getObjectList(OBJ_CONSTRAINT);

				for(auto &obj : (*objects))
				{
					constr=dynamic_cast<Constraint *>(obj);

					/* If the constraint is not FK but is declared outside table via alter (ALTER TABLE...ADD CONSTRAINT...) or
		   The constraint is FK and the reference table is disabled the FK will not be enabled */
					if((constr->getConstraintType()!=ConstraintType::foreign_key && !constr->isDeclaredInTable()) ||
							(constr->getConstraintType()==ConstraintType::foreign_key &&
							 (disable || (!disable && !constr->getReferencedTable()->isSQLDisabled()))))
						constr->setSQLDisabled(disable);
				}
			}
		}
	}

	void disableReferencesSQL(BaseObject *object)
	{
		if(object && object->getDatabase())
		{
			vector<BaseObject *> refs;
			TableObject *tab_obj=nullptr;
			DatabaseModel *model=dynamic_cast<DatabaseModel *>(object->getDatabase());

			model->getObjectReferences(object, refs);

			while(!refs.empty())
			{
				tab_obj=dynamic_cast<TableObject *>(refs.back());

				//If the object is a relationship added does not do anything since the relationship itself will be disabled
				if(refs.back()->getObjectType()!=BASE_RELATIONSHIP &&
						(!tab_obj || (tab_obj && !tab_obj->isAddedByRelationship())))
				{
					refs.back()->setSQLDisabled(object->isSQLDisabled());

					//Update the parent table graphical representation to show the disabled child object
					if(tab_obj)
						tab_obj->getParentTable()->setModified(true);

					//Disable the references of the current object too
					disableReferencesSQL(refs.back());
				}

				refs.pop_back();
			}
		}
	}

	QString formatMessage(const QString &msg)
	{
		QString fmt_msg=msg;
		QChar start_chrs[2]={'`','('},
				end_chrs[2]={'\'', ')'};
		QStringList start_tags={ QString("<strong>"), QString("<em>(") },
				end_tags={ QString("</strong>"), QString(")</em>") };
		int pos=-1, pos1=-1;

		// Replacing the form `' by <strong></strong> and () by <em></em>
		for(int chr_idx=0; chr_idx < 2; chr_idx++)
		{
			pos=0;
			do
			{
				pos=fmt_msg.indexOf(start_chrs[chr_idx], pos);
				pos1=fmt_msg.indexOf(end_chrs[chr_idx], pos);

				if(pos >= 0 && pos1 >=0)
				{
					fmt_msg.replace(pos, 1 , start_tags[chr_idx]);
					pos1 += start_tags[chr_idx].length() - 1;
					fmt_msg.replace(pos1, 1, end_tags[chr_idx]);
				}
				else
					break;

				pos=pos1;
			}
			while(pos >= 0 && pos < fmt_msg.size());
		}

		fmt_msg.replace(QString("\n"), QString("<br/>"));

		return(fmt_msg);
	}

	void configureWidgetFont(QWidget *widget, unsigned factor_id)
	{
		float factor = 1;

		switch(factor_id)
		{
			case SMALL_FONT_FACTOR:
				factor=0.85f;
			break;
			case MEDIUM_FONT_FACTOR:
				factor=0.90f;
			break;
			case BIG_FONT_FACTOR:
				factor=1.10f;
			break;
			case HUGE_FONT_FACTOR:
			default:
				factor=1.40f;
			break;
		}

		configureWidgetFont(widget, factor);
	}

	void configureWidgetFont(QWidget *widget, float factor)
	{
		if(!widget)
			return;

		QFont font=widget->font();
		font.setPointSizeF(font.pointSizeF() * factor);
		widget->setFont(font);
	}
}
