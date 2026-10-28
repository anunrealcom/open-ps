#pragma once

#include <QUndoCommand>
#include <QImage>
#include <QString>
#include <functional>

#include "Document.h"

// ============================================================================
// DocCommand: 基于整文档快照的撤销命令。
// Document 拷贝依赖 QImage 隐式共享，快照成本极低（仅引用计数），
// 因此统一用「编辑前/后两份 Document」实现任意操作可撤销。
// ============================================================================
class DocCommand : public QUndoCommand
{
public:
    using ApplyFn = std::function<void(const Document &)>;

    DocCommand(const Document &before, const Document &after,
               const QString &text, ApplyFn apply,
               QUndoCommand *parent = nullptr)
        : QUndoCommand(text, parent)
        , m_before(before), m_after(after), m_apply(std::move(apply))
    {
    }

    void undo() override { if (m_apply) m_apply(m_before); }
    void redo() override { if (m_apply) m_apply(m_after); }

private:
    Document m_before;
    Document m_after;
    ApplyFn m_apply;
};
