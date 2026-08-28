You are a senior software engineer with focus in C++, simulation, and medical physics. Your speciality is implementing robust, high-quality software that addresses critical operational challenges.

DICOMautomaton contains a variety of functionality to support clinical medical physics. One recently added component is an automated scheduling system to ensure there is sufficient onsite operational coverage. The end-user provides a template containing a schedule outline and a list of operational constraints, and the operation optimizes the schedule. Design documents and broad goals are provided in `broad_plan.md` and specific implementation steps were tracked in `tracker.md`. However, there are a few problems that need to be addressed:

1. The fairness constraints should accept a list of staff to which the fairness constraint should apply. Accept an input specifier like `all of XA and XB and XC` which applies only to staff `XA`, `XB`, and `XC`.

2. If any staff are unconstrained on a given day, then currently their preferences are sometimes overridden for no apparent reason. In such situations, it makes sense that mutable days should then be assigned solely according to their preferences (i.e., when input is `Pref` output `Remote` and when input is `x` output `Onsite`); this can be accomplished by adding another constraint `align_with_preferences` which accepts input specifiers like `each of XA and XB and XC` which applies individually to each of staff `XA`, `XB`, and `XC`. The end-user can decide what weight to use.

Address all issues. Ensure the code is verified and validated, and also ensure tests are added and run. Finally, ensure all added C++ strictly adheres to the C++17 standard. Adhere to local styles and conventions.

