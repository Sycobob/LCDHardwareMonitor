﻿using System.ComponentModel;
using OpenHardwareMonitor.Hardware;

namespace LCDHardwareMonitor
{
	/// <summary>
	/// Exposes an <see cref="OpenHardwareMonitor.Hardware.ISensor"/> for
	/// display in XAML.
	/// </summary>
	public class SensorNode : INode, INotifyPropertyChanged
	{
		#region Constructor

		public SensorNode ( ISensor sensor )
		{
			Sensor = sensor;

			App.Tick += OnTick;
		}

		#endregion

		#region Public Interface

		public ISensor Sensor { get; private set; }

		/// <summary>
		/// Cleanup when this node is removed from the tree. Namely, unregister
		/// from events to prevent memory leaks.
		/// </summary>
		public void RemovedFromTree ()
		{
			App.Tick -= OnTick;
		}

		#endregion

		#region INotifyPropertyChanged Implementation

		public event PropertyChangedEventHandler PropertyChanged;

		//OPTIMIZE: This is lazy. Not sure if it's a performance issue.
		/// <summary>
		/// Just flag the entire sensor as changed every tick.
		/// </summary>
		private void OnTick ()
		{
			if ( PropertyChanged != null )
			{
				var args = new PropertyChangedEventArgs(null);
				PropertyChanged(this, args);
			}
		}

		#endregion
	}
}