﻿namespace LCDHardwareMonitor
{
	using System;
	using System.Collections.ObjectModel;
	using System.Windows;
	using System.Windows.Threading;
	using LCDHardwareMonitor.Drawables;
	using OpenHardwareMonitor.GUI;
	using OpenHardwareMonitor.Hardware;

	/// <summary>
	/// Interaction logic for App.xaml
	/// </summary>
	public partial class App : Application
	{
		public static Computer Computer { get; private set; }
		public static event Action Tick;

		public static ReadOnlyObservableCollection<IWidget> Widgets { get; private set; }
		public static         ObservableCollection<IWidget> widgets = new ObservableCollection<IWidget>();

		private IVisitor updateVisitor = new UpdateVisitor();

		public App ()
		{
			Computer = InitializeComputer();

			//DEBUG
			widgets.Add(new Widget(new StaticText()));

			Widgets = new ReadOnlyObservableCollection<IWidget>(widgets);

			/* Update the OHM data at a regular interval as long as the
			 * application is running.
			 */
			var timer = new DispatcherTimer();
			//TODO: Settings
			timer.Interval = new TimeSpan(0, 0, 0, 0, 800);
			timer.Tick += Update;
			timer.Start();
		}

		//TODO: Only update sensors that are actually in use (and make OHMDataPage responsible for its own updates)

		private void Update ( object sender, EventArgs e )
		{
			Computer.Accept(updateVisitor);

			if ( Tick != null )
				Tick();
		}

		private Computer InitializeComputer ()
		{
			Computer computer = new Computer();
			computer.Open();

			//TODO: Saved settings fodder
			//ENABLE ALL THE THINGS!
			computer.CPUEnabled = true;
			computer.FanControllerEnabled = true;
			computer.GPUEnabled = true;
			computer.HDDEnabled = true;
			computer.MainboardEnabled = true;
			computer.RAMEnabled = true;

			return computer;
		}
	}
}