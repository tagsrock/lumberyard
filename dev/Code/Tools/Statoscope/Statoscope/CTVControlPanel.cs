/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace Statoscope
{
	partial class CTVControlPanel : UserControl
	{
		CheckboxTreeView m_ctv;
		public CheckboxTreeView CTV
		{
			get
			{
				return m_ctv;
			}

			set
			{
				m_ctv = value;

				if (CTV != null)
				{
					CTV.HistoryStateListener += HistoryStateChanged;
					UpdateButtons();
				}
			}
		}

		public CTVControlPanel()
		{
			InitializeComponent();
		}

		public CTVControlPanel(CheckboxTreeView ctv)
		{
			InitializeComponent();
			CTV = ctv;
		}

		private void undoButton_Click(object sender, EventArgs e)
		{
			CTV.Undo();
			UpdateButtons();
		}

		private void redoButton_Click(object sender, EventArgs e)
		{
			CTV.Redo();
			UpdateButtons();
		}

		private void filterText_TextChanged(object sender, EventArgs e)
		{
			CTV.FilterTextChanged(filterText.Text.ToUpper());
		}

		void UpdateButtons()
		{
			undoButton.Enabled = CTV.NumUndoStates > 0;
			redoButton.Enabled = CTV.NumRedoStates > 0;
		}

		public void HistoryStateChanged(object sender, EventArgs e)
		{
			UpdateButtons();
		}
	}
}
