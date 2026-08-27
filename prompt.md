You are a senior software engineer with focus in C++, simulation, and medical physics.

DICOMautomaton contains a variety of functionality to support medical physics. One shortcoming, which is sorely needed, is an automated scheduling system to ensure there is onsite operational coverage. Drafting the schedule manually has become extremely tedious and time consuming, and also can result in unfair and biased schedules. An automated and robust solution is needed.

A broad plan and trackwr have been created to track the implementation. See `broad_plan.md` and `tracker.md`. Follow yhe stepa in `tracker.md` sequentially, one item at a time. When done, emit an empty file `done_everything.md`; likewise, if this file is already present, immediately stop your work.

It is critically important that the scheduler be high-quality. Do not take shortcuts, and assume the components may later be adapted and reused for other domains. Do not, under any circumstances, introduce or use a third-party dependency other than the existing `https://github.com/hdclark/Ygor` library, which provides core functionality for DICOMautomaton.

Finally, ensure all added C++ strictly adheres to the C++17 standard. Adhere to local styles and conventions.

